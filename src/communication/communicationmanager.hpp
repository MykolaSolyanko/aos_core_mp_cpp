/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef COMMUNICATIONMANAGER_HPP_
#define COMMUNICATIONMANAGER_HPP_

#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

#include "cmconnection.hpp"
#include "communicationchannel.hpp"
#include "config/config.hpp"
#include "iamconnection.hpp"
#include "types.hpp"

/**
 * Communication manager class.
 */
class CommunicationManager : public CommunicationManagerItf {
public:
    /**
     * Initialize communication manager.
     *
     * @param cfg Configuration
     * @param transport Transport
     * @param iamChannel IAM channel
     * @param cmChannel CM channel
     * @param certProvider Certificate provider
     * @param certLoader Certificate loader
     * @param cryptoProvider Crypto provider
     * @return aos::Error
     */
    aos::Error Init(const Config& cfg, TransportItf& transport, CertProviderItf* certProvider = nullptr,
        aos::cryptoutils::CertLoaderItf* certLoader     = nullptr,
        aos::crypto::x509::ProviderItf*  cryptoProvider = nullptr);

    /**
     * Create communication channel.
     *
     * @param port Port
     * @param certProvider Certificate provider
     * @return std::unique_ptr<CommChannelItf>
     */
    std::unique_ptr<CommChannelItf> CreateChannel(
        int port, CertProviderItf* certProvider, const std::string& certStorage) override;

    /**
     * Connect to the communication manager.
     *
     * @return aos::Error
     */
    aos::Error Connect() override;

    /**
     * Read message from the communication manager.
     *
     * @param message Message
     * @return aos::Error
     */
    aos::Error Read(std::vector<uint8_t>& message) override;

    /**
     * Write message to the communication manager.
     *
     * @param message Message
     * @return aos::Error
     */
    aos::Error Write(std::vector<uint8_t> message) override;

    /**
     * Close the communication manager.
     *
     * @return aos::Error
     */
    aos::Error Close() override;

private:
    static constexpr auto                      cMaxMessageSize    = 64 * 1024; // 64 KB
    static constexpr std::chrono::milliseconds cConnectionTimeout = std::chrono::seconds(10);
    static constexpr std::chrono::seconds      cReconnectTimeout  = std::chrono::seconds(3);

    void       Run();
    aos::Error ReadHandler();

    TransportItf*                    mTransport {};
    CertProviderItf*                 mCertProvider {};
    aos::cryptoutils::CertLoaderItf* mCertLoader;
    aos::crypto::x509::ProviderItf*  mCryptoProvider;
    const Config*                    mCfg;
    // IAMConnection                                        mIAMConnection;
    // CMConnection                                         mCMConnection;
    std::map<int, std::unique_ptr<CommunicationChannel>> mChannels;
    std::thread                                          mThread;
    std::atomic<bool>                                    mShutdown {};
    bool                                                 mIsConnected {};
    std::mutex                                           mMutex;
    std::condition_variable                              mCondVar;
};

#endif /* COMMUNICATIONMANAGER_HPP_ */
