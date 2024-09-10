/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DOWNLOADER_HPP_
#define DOWNLOADER_HPP_

#include <functional>
#include <string>

#include <Poco/TaskManager.h>
#include <Poco/URI.h>

#include <aos/common/tools/error.hpp>

/**
 * Downloader.
 */
class Downloader {
public:
    /**
     * Finished callback.
     *
     * @param url URL.
     * @param error Error.
     */
    using FinishedCallback = std::function<void(const std::string&, aos::Error)>;

    /**
     * Constructor.
     *
     * @param downloadDir download directory.
     */
    Downloader(const std::string& downloadDir);

    /**
     * Destructor.
     */
    ~Downloader();

    /**
     * Download file synchronously.
     *
     * @param url URL.
     * @return aos::RetWithError<std::string>.
     */
    aos::RetWithError<std::string> DownloadSync(const std::string& url);

    /**
     * Download file asynchronously.
     *
     * @param url URL.
     * @param callback callback.
     */
    void DownloadAsync(const std::string& url, FinishedCallback callback);

private:
    constexpr static std::chrono::milliseconds cDelay {1000};
    constexpr static std::chrono::milliseconds cMaxDelay {5000};
    constexpr static int                       cMaxRetryCount {3};
    constexpr static int                       cTimeoutSec {10};

    aos::Error Download(const std::string& url, const std::string& outfilename);
    aos::Error DownloadFile(const Poco::URI& uri, const std::string& outfilename);
    aos::Error RetryDownload(const std::string& url, const std::string& outfilename);

    std::string       mDownloadDir;
    Poco::TaskManager mTaskManager;
};

#endif // DOWNLOADER_HPP_
