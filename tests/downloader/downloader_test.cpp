/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <future>
#include <optional>
#include <thread>

#include <gtest/gtest.h>

#include "downloader/downloader.hpp"
#include "stubs/httpserver.hpp"

using namespace testing;

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class DownloaderTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        std::ofstream ofs("test_file.dat", std::ios::binary);
        ofs << "This is a test file";
        ofs.close();
    }

    void StartServer()
    {
        mServer.emplace("test_file.dat", 8000);
        mServer->Start();
    }

    void StopServer() { mServer->Stop(); }

    void TearDown() override
    {
        std::remove("test_file.dat");
        std::remove("download/test_file.dat");
    }

    std::optional<HTTPServer> mServer;
    std::string               mDownloadDir = "download";
    Downloader                mDownloader  = Downloader(mDownloadDir);
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(DownloaderTest, DownloadSync)
{
    StartServer();
    auto result = mDownloader.DownloadSync("http://localhost:8000/test_file.dat");
    EXPECT_EQ(result.mError, aos::ErrorEnum::eNone);
    EXPECT_EQ(result.mValue, "download/test_file.dat");

    std::ifstream ifs(result.mValue, std::ios::binary);
    std::string   content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    EXPECT_EQ(content, "This is a test file");

    StopServer();
}

TEST_F(DownloaderTest, DownloadAsync)
{
    std::promise<void> promise;
    auto               future = promise.get_future();

    StartServer();

    mDownloader.DownloadAsync(
        "http://localhost:8000/test_file.dat", [&](const std::string& filename, aos::Error error) {
            EXPECT_EQ(error, aos::ErrorEnum::eNone);
            EXPECT_EQ(filename, "download/test_file.dat");

            std::ifstream ifs(filename, std::ios::binary);
            std::string   content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
            EXPECT_EQ(content, "This is a test file");

            promise.set_value();
        });

    auto status = future.wait_for(std::chrono::seconds(5));

    EXPECT_EQ(status, std::future_status::ready);

    StopServer();
}

TEST_F(DownloaderTest, DownloadFileScheme)
{
    auto result = mDownloader.DownloadSync("file://test_file.dat");
    EXPECT_EQ(result.mError, aos::ErrorEnum::eNone);
    EXPECT_EQ(result.mValue, "download/test_file.dat");

    std::ifstream ifs(result.mValue, std::ios::binary);
    std::string   content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    EXPECT_EQ(content, "This is a test file");
}
