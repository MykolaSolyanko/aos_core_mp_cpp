/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <utils/grpchelper.hpp>

#include "logger/logmodule.hpp"
#include "publicnodeclient.hpp"

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

aos::Error PublicNodeClient::Init(const IAMConfig& cfg, CertProviderItf& certProvider, bool publicServer)
{
    LOG_INF() << "Initializing PublicNodeClient: publicServer=" << publicServer;

    if (auto err = CreateCredentials(cfg.mCertStorage, certProvider, publicServer); !err.IsNone()) {
        return err;
    }

    mUrl = publicServer ? cfg.mIAMPublicServerURL : cfg.mIAMProtectedServerURL;

    mPublicServer = publicServer;

    return aos::ErrorEnum::eNone;
}

aos::Error PublicNodeClient::SendMessages(std::vector<uint8_t> messages)
{
    LOG_DBG() << "Sending messages";

    return mOutgoingMsgChannel.Send(std::move(messages));
}

aos::RetWithError<std::vector<uint8_t>> PublicNodeClient::ReceiveMessages()
{
    LOG_DBG() << "Receiving messages";

    return mIncomingMsgChannel.Receive();
}

void PublicNodeClient::OnConnected()
{
    std::lock_guard<std::mutex> lock {mMutex};

    if (!mNotifyConnected) {
        mNotifyConnected = true;

        mConnectionThread          = std::thread(&PublicNodeClient::ConnectionLoop, this, mUrl);
        mHandlerOutgoingMsgsThread = std::thread(&PublicNodeClient::ProcessOutgoingIAMMessages, this);
    }
}

void PublicNodeClient::OnDisconnected()
{
    Close();
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

void PublicNodeClient::Close()
{
    LOG_INF() << "Destroying PublicNodeClient";

    {
        std::unique_lock lock {mMutex};

        if (mShutdown || !mNotifyConnected) {
            return;
        }

        mShutdown        = true;
        mNotifyConnected = false;

        if (mRegisterNodeCtx) {
            mRegisterNodeCtx->TryCancel();
        }
    }

    mCV.notify_all();

    mOutgoingMsgChannel.Close();
    mIncomingMsgChannel.Close();

    if (mConnectionThread.joinable()) {
        mConnectionThread.join();
    }

    if (mHandlerOutgoingMsgsThread.joinable()) {
        mHandlerOutgoingMsgsThread.join();
    }
}

aos::Error PublicNodeClient::CreateCredentials(
    const std::string& certStorage, CertProviderItf& certProvider, bool publicServer)
{
    if (publicServer) {
        mCredentialList.push_back(grpc::InsecureChannelCredentials());

        if (auto TLSCredentials = certProvider.GetTLSCredentials(); TLSCredentials) {
            mCredentialList.push_back(certProvider.GetTLSCredentials());
        }

        return aos::ErrorEnum::eNone;
    }

    auto res = certProvider.GetMTLSConfig(certStorage);
    if (!res.mError.IsNone()) {
        return AOS_ERROR_WRAP(res.mError);
    }

    mCredentialList.push_back(res.mValue);

    return aos::ErrorEnum::eNone;
}

void PublicNodeClient::ConnectionLoop(const std::string& url) noexcept
{
    LOG_DBG() << "PublicNodeClient connection loop started";

    while (!mShutdown) {
        try {
            if (auto err = RegisterNode(url); !err.IsNone()) {
                LOG_ERR() << "Failed to register node: error=" << err.Message();
            }
        } catch (const std::exception& e) {
            LOG_ERR() << "PublicNodeClient connection error: " << e.what();
        }

        std::unique_lock lock {mMutex};

        mCV.wait_for(lock, cReconnectInterval, [this]() { return mShutdown.load(); });
    }

    LOG_DBG() << "PublicNodeClient connection loop stopped";
}

aos::Error PublicNodeClient::RegisterNode(const std::string& url)
{
    LOG_DBG() << "Registering node: url=" << url.c_str();

    for (const auto& credentials : mCredentialList) {
        {
            std::unique_lock lock {mMutex};

            if (mShutdown) {
                return aos::ErrorEnum::eNone;
            }

            auto channel = grpc::CreateCustomChannel(url, credentials, grpc::ChannelArguments());
            if (!channel) {
                LOG_ERR() << "Failed to create channel";

                continue;
            }

            mPublicNodeServiceStub = PublicNodeService::NewStub(channel);
            if (!mPublicNodeServiceStub) {
                LOG_ERR() << "Failed to create stub";

                continue;
            }

            mRegisterNodeCtx = std::make_unique<grpc::ClientContext>();
            mStream          = mPublicNodeServiceStub->RegisterNode(mRegisterNodeCtx.get());
            if (!mStream) {
                LOG_ERR() << "Failed to create stream";

                continue;
            }

            mConnected = true;
            mCV.notify_all();

            LOG_DBG() << "Connection established";
        }

        if (auto err = SendCachedMessages(); !err.IsNone()) {
            LOG_ERR() << "Failed to send cached messages: error=" << err.Message();

            continue;
        }

        LOG_DBG() << "Try handling incoming messages url=" << url.c_str();

        if (auto err = HandleIncomingMessages(); !err.IsNone()) {
            LOG_ERR() << "Failed to handle incoming messages: error=" << err.Message();
        }
    }

    return aos::Error(aos::ErrorEnum::eRuntime, "failed to register node");
}

aos::Error PublicNodeClient::HandleIncomingMessages()
{
    iamanager::v5::IAMIncomingMessages incomingMsg;

    LOG_DBG() << "Handling incoming messages";

    while (mStream->Read(&incomingMsg)) {
        std::vector<uint8_t> message(incomingMsg.ByteSizeLong());

        LOG_DBG() << "Received message: msg=" << incomingMsg.DebugString().c_str();

        if (!incomingMsg.SerializeToArray(message.data(), message.size())) {
            LOG_ERR() << "Failed to serialize message";

            continue;
        }

        if (auto err = mIncomingMsgChannel.Send(std::move(message)); !err.IsNone()) {
            return aos::Error(aos::ErrorEnum::eRuntime, "failed to send message");
        }
    }

    return aos::ErrorEnum::eNone;
}

void PublicNodeClient::ProcessOutgoingIAMMessages()
{
    LOG_DBG() << "Processing outgoing IAM messages";

    while (!mShutdown) {
        auto [msg, err] = mOutgoingMsgChannel.Receive();
        if (!err.IsNone()) {
            LOG_ERR() << "Failed to receive message: error=" << err;

            return;
        }

        LOG_DBG() << "Received message from IAM";

        {
            std::unique_lock<std::mutex> lock(mMutex);
            mCV.wait(lock, [this] { return mConnected || mShutdown; });

            if (mShutdown) {
                return;
            }
        }

        iamanager::v5::IAMOutgoingMessages outgoingMsg;
        if (!outgoingMsg.ParseFromArray(msg.data(), static_cast<int>(msg.size()))) {
            LOG_ERR() << "Failed to parse outgoing message";

            continue;
        }

        LOG_DBG() << "Sending message to IAM: msg=" << outgoingMsg.DebugString().c_str();

        if (!mStream->Write(outgoingMsg)) {
            LOG_ERR() << "Failed to send message";
            CacheMessage(outgoingMsg);

            continue;
        }
    }
}

void PublicNodeClient::CacheMessage(const iamanager::v5::IAMOutgoingMessages& message)
{
    std::lock_guard<std::mutex> lock(mMutex);

    LOG_DBG() << "Caching message";
    mMessageCache.push(message);
}

aos::Error PublicNodeClient::SendCachedMessages()
{
    std::lock_guard<std::mutex> lock(mMutex);

    while (!mMessageCache.empty()) {
        const auto& message = mMessageCache.front();

        if (!mStream->Write(message)) {
            return aos::Error(aos::ErrorEnum::eRuntime, "failed to send cached message");
        }

        mMessageCache.pop();

        LOG_DBG() << "Cached message sent";
    }

    return aos::ErrorEnum::eNone;
}
