/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <filesystem>
#include <fstream>
#include <optional>
#include <regex>
#include <stdexcept>
#include <vector>

#include <Poco/DigestEngine.h>
#include <Poco/DigestStream.h>
#include <Poco/SHA2Engine.h>
#include <Poco/StreamCopier.h>

#include <utils/filesystem.hpp>
#include <utils/image.hpp>
#include <utils/json.hpp>

#include "logger/logmodule.hpp"
#include "serviceimage.hpp"

namespace fs = std::filesystem;

/***********************************************************************************************************************
 * Consts
 **********************************************************************************************************************/

namespace {
const char* cBlobsFolder  = "blobs";
const char* cManifestFile = "manifest.json";
const char* cTmpRootFSDir = "tmprootfs";
const int   cBufferSize   = 1024 * 1024; // 1MB
} // namespace

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

struct Descriptor {
    aos::common::utils::Digest mDigest;
};

struct Manifest {
    Descriptor              mConfig;
    std::vector<Descriptor> mLayers;
};

struct ServiceManifest {
    std::optional<Descriptor> mAosService;
    Manifest                  mManifest;
};

struct ImageParts {
    std::string mImageConfigPath;
    std::string mServiceConfigPath;
    std::string mServiceFSPath;
};

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

static Manifest ParseManifest(const aos::common::utils::CaseInsensitiveObjectWrapper& object)
{
    LOG_DBG() << "Parsing manifest";

    Manifest manifest;

    manifest.mConfig.mDigest = object.GetObject("config").GetValue<std::string>("digest");
    manifest.mLayers
        = aos::common::utils::GetArrayValue<Descriptor>(object, "layers", [](const Poco::Dynamic::Var& value) {
              aos::common::utils::CaseInsensitiveObjectWrapper layers(value.extract<Poco::JSON::Object::Ptr>());

              return Descriptor {layers.GetValue<std::string>("digest")};
          });

    return manifest;
}

static auto ParseJson(const std::string& path)
{
    LOG_DBG() << "Parsing json: path=" << path.c_str();

    std::ifstream file(path);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open manifest file: " + path);
    }

    auto res = aos::common::utils::ParseJson(file);
    if (!res.mError.IsNone()) {
        throw std::runtime_error("Failed to parse manifest file: " + path);
    }

    return res.mValue.extract<Poco::JSON::Object::Ptr>();
}

static ServiceManifest ParseServiceManifest(const std::string& manifestPath)
{
    LOG_DBG() << "Parsing manifest: manifestPath=" << manifestPath.c_str();

    aos::common::utils::CaseInsensitiveObjectWrapper object(ParseJson(manifestPath));
    ServiceManifest                                  serviceManifest;

    serviceManifest.mManifest = ParseManifest(object);

    if (object.Has("aosService")) {
        serviceManifest.mAosService = Descriptor {object.GetObject("aosService").GetValue<std::string>("digest")};
    }

    return serviceManifest;
}

static ImageParts GetImageParts(const std::string& imagePath, const ServiceManifest& manifest)
{
    LOG_DBG() << "Getting image parts: imagePath=" << imagePath.c_str();

    ImageParts parts;
    auto [algorithm, hex] = aos::common::utils::ParseDigest(manifest.mManifest.mConfig.mDigest);

    parts.mImageConfigPath = fs::path(imagePath) / cBlobsFolder / algorithm / hex;

    if (manifest.mAosService.has_value()) {
        std::tie(algorithm, hex) = aos::common::utils::ParseDigest(manifest.mAosService->mDigest);
        parts.mServiceConfigPath = fs::path(imagePath) / cBlobsFolder / algorithm / hex;
    }

    std::tie(algorithm, hex) = aos::common::utils::ParseDigest(manifest.mManifest.mLayers[0].mDigest);
    parts.mServiceFSPath     = fs::path(imagePath) / cBlobsFolder / algorithm / hex;

    return parts;
}

static std::string ExtractImage(const std::string& archivePath, const std::string& imageStoreDir)
{
    LOG_DBG() << "Extracting image: archivePath=" << archivePath.c_str() << ",imageStoreDir=" << imageStoreDir.c_str();

    auto [value, error] = aos::common::utils::MkTmpDir(imageStoreDir);
    if (!error.IsNone()) {
        throw std::runtime_error("Failed to create temporary directory for image: " + std::string(error.Message()));
    }

    if (auto err = aos::common::utils::UnpackTarImage(archivePath, value); !err.IsNone()) {
        throw std::runtime_error("Failed to unpack image: " + std::string(err.Message()));
    }

    return value;
}

static void ValidateImage(const std::string& imagePath, const ServiceManifest& serviceManifest)
{
    LOG_DBG() << "Validating image: imagePath=" << imagePath.c_str();

    if (auto err = aos::common::utils::ValidateDigest(serviceManifest.mManifest.mConfig.mDigest); !err.IsNone()) {
        throw std::runtime_error("Invalid image config digest: " + std::string(err.Message()));
    }

    if (serviceManifest.mAosService.has_value()) {
        if (auto err = aos::common::utils::ValidateDigest(serviceManifest.mAosService->mDigest); !err.IsNone()) {
            throw std::runtime_error("Invalid aos service digest: " + std::string(err.Message()));
        }

        auto [algorithm, hex] = aos::common::utils::ParseDigest(serviceManifest.mAosService->mDigest);

        // this used only for validation json
        ParseJson(fs::path(imagePath) / cBlobsFolder / algorithm / hex);
    }

    auto [algorithm, hex] = aos::common::utils::ParseDigest(serviceManifest.mManifest.mLayers[0].mDigest);
    auto rootfsPath       = fs::path(imagePath) / cBlobsFolder / algorithm / hex;

    if (!fs::exists(rootfsPath)) {
        throw std::runtime_error("Rootfs not found: " + rootfsPath.string());
    }

    if (!fs::is_directory(rootfsPath)) {
        if (auto err = aos::common::utils::ValidateDigest(serviceManifest.mManifest.mLayers[0].mDigest);
            !err.IsNone()) {
            throw std::runtime_error("Invalid rootfs digest: " + std::string(err.Message()));
        }

        return;
    }

    auto [digest, error] = aos::common::utils::HashDir(rootfsPath);
    if (!error.IsNone()) {
        throw std::runtime_error("Failed to calculate rootfs checksum: " + digest);
    }

    if (serviceManifest.mManifest.mLayers[0].mDigest != digest) {
        throw std::runtime_error("Incorrect rootfs checksum");
    }
}

static aos::common::utils::Digest PrepareServiceFS(const std::string& imagePath, const ServiceManifest& serviceManifest)
{
    LOG_DBG() << "Preparing service FS: imagePath=" << imagePath.c_str();

    auto imageParts = GetImageParts(imagePath, serviceManifest);

    auto tmpRootFS = fs::path(imagePath) / cTmpRootFSDir;
    fs::create_directory(tmpRootFS);

    if (auto err = aos::common::utils::UnpackTarImage(imageParts.mServiceFSPath, tmpRootFS); !err.IsNone()) {
        throw std::runtime_error("Failed to unpack service FS: " + std::string(err.Message()));
    }

    fs::remove_all(imageParts.mServiceFSPath);

    auto [rootFSDigest, error] = aos::common::utils::HashDir(tmpRootFS);
    if (!error.IsNone()) {
        throw std::runtime_error("Failed to calculate rootfs checksum: " + rootFSDigest);
    }

    aos::common::utils::ValidateDigest(rootFSDigest);

    auto [_, hex] = aos::common::utils::ParseDigest(rootFSDigest);

    fs::rename(tmpRootFS, fs::path(imageParts.mServiceFSPath).parent_path() / hex);

    return rootFSDigest;
}

static void UpdateRootFSDigestInManifest(
    const std::string& manifestPath, const aos::common::utils::Digest& rootFSDigest, ServiceManifest& serviceManifest)
{
    LOG_DBG() << "Updating root FS digest in manifest: manifestPath=" << manifestPath.c_str();

    serviceManifest.mManifest.mLayers[0].mDigest = rootFSDigest;

    auto manifest = ParseJson(manifestPath);

    aos::common::utils::CaseInsensitiveObjectWrapper object(manifest);

    object.GetArray("layers")->getObject(0)->set("digest", rootFSDigest);

    if (auto err = aos::common::utils::WriteJsonToFile(object, manifestPath); !err.IsNone()) {
        throw std::runtime_error("Failed to update manifest");
    }
}

/***********************************************************************************************************************
 * Public functions
 **********************************************************************************************************************/

aos::RetWithError<std::string> UnpackService(const std::string& archivePath, const std::string& imageStoreDir)
{
    LOG_DBG() << "Unpacking service image: archivePath=" << archivePath.c_str()
              << ",imageStoreDir=" << imageStoreDir.c_str();

    std::string imagePath;

    try {
        imagePath = ExtractImage(archivePath, imageStoreDir);

        auto serviceManifest = ParseServiceManifest(fs::path(imagePath) / cManifestFile);

        ValidateImage(imagePath, serviceManifest);

        UpdateRootFSDigestInManifest(
            fs::path(imagePath) / cManifestFile, PrepareServiceFS(imagePath, serviceManifest), serviceManifest);
    } catch (const std::exception& e) {
        return {{}, aos::Error(aos::ErrorEnum::eRuntime, e.what())};
    }

    LOG_DBG() << "Service image unpacked: " << imagePath.c_str();

    return {imagePath};
}