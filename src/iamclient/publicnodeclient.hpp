/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PUBLICNODECLIENT_HPP_
#define PUBLICNODECLIENT_HPP_

#include <atomic>
#include <condition_variable>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include <grpcpp/channel.h>
#include <grpcpp/security/credentials.h>

#include <iamanager/v5/iamanager.grpc.pb.h>

#include <aos/common/tools/error.hpp>
#include <utils/channel.hpp>

#include "communication/types.hpp"
#include "config/config.hpp"
#include "types.hpp"
#include "utils/time.hpp"

/**
 * Public node client interface.
 */
class PublicNodeClient : public HandlerItf {
public:
    /**
     * Initialize the client.
     *
     * @param cfg Configuration.
     * @param certProvider Certificate provider.
     * @param publicServer Public server.
     * @return Error error code.
     */
    aos::Error Init(const IAMConfig& cfg, CertProviderItf& certProvider, bool publicServer);

    /**
     * Notify that connection is established.
     */
    void OnConnected() override;

    /**
     * Notify that connection is lost.
     */
    void OnDisconnected() override;

    /**
     * Send messages.
     *
     * @param messages Messages.
     * @return Error error code.
     */
    aos::Error SendMessages(std::vector<uint8_t> messages) override;

    /**
     * Receive messages.
     *
     * @return Messages.
     */
    aos::RetWithError<std::vector<uint8_t>> ReceiveMessages() override;

private:
    using StreamPtr = std::unique_ptr<
        grpc::ClientReaderWriterInterface<iamanager::v5::IAMOutgoingMessages, iamanager::v5::IAMIncomingMessages>>;
    using PublicNodeService        = iamanager::v5::IAMPublicNodesService;
    using PublicNodeServiceStubPtr = std::unique_ptr<PublicNodeService::StubInterface>;
    using HandlerFunc              = std::function<aos::Error(const iamanager::v5::IAMIncomingMessages&)>;

    static constexpr auto cReconnectInterval = std::chrono::seconds(3);

    aos::Error CreateCredentials(const std::string& certStorage, CertProviderItf& certProvider, bool publicServer);
    void       ConnectionLoop(const std::string& url) noexcept;
    aos::Error HandleIncomingMessages();
    aos::Error RegisterNode(const std::string& url);
    void       InitializeHandlers();
    void       ProcessOutgoingIAMMessages();
    void       Close();
    void       CacheMessage(const iamanager::v5::IAMOutgoingMessages& message);
    aos::Error SendCachedMessages();

    std::vector<std::shared_ptr<grpc::ChannelCredentials>> mCredentialList;

    std::unique_ptr<grpc::ClientContext> mRegisterNodeCtx;
    StreamPtr                            mStream;
    PublicNodeServiceStubPtr             mPublicNodeServiceStub;

    std::thread             mConnectionThread;
    std::thread             mHandlerOutgoingMsgsThread;
    std::condition_variable mCV;
    std::atomic<bool>       mShutdown {};
    bool                    mConnected {};
    bool                    mNotifyConnected {};
    std::mutex              mMutex;
    std::string             mUrl;
    bool                    mPublicServer;

    aos::common::utils::Channel<std::vector<uint8_t>> mOutgoingMsgChannel;
    aos::common::utils::Channel<std::vector<uint8_t>> mIncomingMsgChannel;

    std::queue<iamanager::v5::IAMOutgoingMessages> mMessageCache;
};

#endif // PUBLICNODECLIENT_HPP_
