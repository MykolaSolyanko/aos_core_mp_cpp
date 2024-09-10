/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>

#include <Poco/FileStream.h>
#include <Poco/StreamCopier.h>
#include <Poco/Task.h>
#include <Poco/ThreadPool.h>
#include <curl/curl.h>

#include <utils/exception.hpp>

#include "downloader.hpp"
#include "logger/logmodule.hpp"

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

class DownloadTask : public Poco::Task {
public:
    using Callback = std::function<void()>;
    DownloadTask(Callback callback)
        : Poco::Task("DownloadTask")
        , mCallback(std::move(callback))
    {
    }

    void runTask() override { mCallback(); }

private:
    Callback mCallback;
};

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

static size_t WriteData(void* ptr, size_t size, size_t nmemb, FILE* stream)
{
    size_t written = fwrite(ptr, size, nmemb, stream);

    return written;
}

static std::string GetFileNameFromURL(const std::string& url)
{
    Poco::URI   uri(url);
    std::string path = uri.getPath();

    if (uri.getScheme() == "file") {
        if (path.empty() && !uri.getHost().empty()) {
            path = "/" + uri.getHost();
        }
    }

    size_t pos = path.find_last_of('/');
    if (pos != std::string::npos) {
        return path.substr(pos + 1);
    }

    return path;
}

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Downloader::Downloader(const std::string& downloadDir)
    : mDownloadDir(downloadDir)
    , mTaskManager(Poco::ThreadPool::defaultPool())
{
    try {
        if (!std::filesystem::exists(mDownloadDir)) {
            std::filesystem::create_directories(mDownloadDir);
        }
    } catch (const std::exception& e) {
        AOS_ERROR_THROW("Failed to create download directory: downloadDir=" + mDownloadDir, aos::ErrorEnum::eFailed);
    }
}

aos::RetWithError<std::string> Downloader::DownloadSync(const std::string& url)
{
    LOG_DBG() << "Sync downloading: url=" << url.c_str();

    std::string outfilename = mDownloadDir + "/" + GetFileNameFromURL(url);

    return {outfilename, RetryDownload(url, outfilename)};
}

void Downloader::DownloadAsync(const std::string& url, FinishedCallback callback)
{
    LOG_DBG() << "Async downloading: url=" << url.c_str();

    std::string outfilename = mDownloadDir + "/" + GetFileNameFromURL(url);

    mTaskManager.start(new DownloadTask([this, url, outfilename, callback]() {
        auto res = RetryDownload(url, outfilename);
        callback(outfilename, res);
    }));
}

Downloader::~Downloader()
{
    mTaskManager.cancelAll();
    mTaskManager.joinAll();
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

aos::Error Downloader::Download(const std::string& url, const std::string& outfilename)
{
    Poco::URI uri(url);
    if (uri.getScheme() == "file") {
        return DownloadFile(uri, outfilename);
    }

    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
    if (!curl) {
        return aos::Error(aos::ErrorEnum::eFailed, "Failed to init curl");
    }

    auto fileCloser = [](FILE* fp) {
        if (fp) {
            if (auto res = fclose(fp); res != 0) {
                LOG_ERR() << "Failed to close file: res=" << res;
            }
        }
    };

    std::unique_ptr<FILE, decltype(fileCloser)> fp(fopen(outfilename.c_str(), "ab"), fileCloser);
    if (!fp) {
        return aos::Error(aos::ErrorEnum::eFailed, "Failed to open file");
    }

    fseek(fp.get(), 0, SEEK_END);
    long existingFileSize = ftell(fp.get());

    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_RESUME_FROM_LARGE, existingFileSize);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, WriteData);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, fp.get());

    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, cTimeoutSec); // Timeout in seconds
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, cTimeoutSec);

    CURLcode res = curl_easy_perform(curl.get());
    if (res != CURLE_OK) {
        return aos::Error(aos::ErrorEnum::eFailed, curl_easy_strerror(res));
    }

    return aos::ErrorEnum::eNone;
}

aos::Error Downloader::DownloadFile(const Poco::URI& uri, const std::string& outfilename)
{
    std::string path = uri.getPath();
    if (path.empty() && !uri.getHost().empty()) {
        path = uri.getHost();
    }

    if (!std::filesystem::exists(path)) {
        return aos::Error(aos::ErrorEnum::eFailed, "File not found");
    }

    try {
        std::ifstream ifs(path, std::ios::binary);
        std::ofstream ofs(outfilename, std::ios::binary | std::ios::trunc);
        ofs << ifs.rdbuf();
    } catch (const std::exception& ex) {
        return aos::Error(aos::ErrorEnum::eFailed, ex.what());
    }

    return aos::ErrorEnum::eNone;
}

aos::Error Downloader::RetryDownload(const std::string& url, const std::string& outfilename)
{
    auto       delay = cDelay;
    aos::Error error;

    for (int retryCount = 0; retryCount < cMaxRetryCount; retryCount++) {
        LOG_DBG() << "Downloading: url=" << url.c_str() << ",retry=" << retryCount;

        if (error = Download(url, outfilename); error.IsNone()) {
            return aos::ErrorEnum::eNone;
        }

        LOG_ERR() << "Failed to download: error=" << error.Message() << ",retry=" << retryCount;

        std::this_thread::sleep_for(delay);

        delay = std::min(delay * 2, cMaxDelay);
    }

    return error;
}
