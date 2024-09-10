/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IAMCONNECTION_HPP_
#define IAMCONNECTION_HPP_

#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

#include <aos/common/tools/error.hpp>

#include "iamclient/types.hpp"
#include "types.hpp"

/**
 * IAM connection class.
 */
class IAMConnection {
public:
    /**
     * Initialize connection.
     *
     * @param port Port.
     * @param certProvider Certificate provider.
     * @param comManager Communication manager.
     * @param channel Channel.
     * @return aos::Error.
     */
    aos::Error Init(int port, HandlerItf& handler, CommunicationManagerItf& comManager,
        CertProviderItf* certProvider = nullptr, const std::string& certStorage = "");

    /**
     * Close connection.
     *
     */
    void Close();

private:
    static constexpr auto cConnectionTimeout = std::chrono::seconds(3);

    void Run();
    void ReadHandler();
    void WriteHandler();

    std::atomic<bool>               mShutdown {};
    std::thread                     mConnectThread;
    std::unique_ptr<CommChannelItf> mIAMCommChannel;
    HandlerItf*                     mHandler {};

    std::mutex              mMutex;
    std::condition_variable mCondVar;
};

#endif /* IAMCONNECTION_HPP_ */
