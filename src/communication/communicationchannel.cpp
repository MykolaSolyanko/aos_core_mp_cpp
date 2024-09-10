#include <thread>

#include "communication/utils.hpp"
#include "communicationchannel.hpp"
#include "logger/logmodule.hpp"

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

std::mutex CommunicationChannel::mCommChannelMutex;

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

CommunicationChannel::CommunicationChannel(int port, CommChannelItf* commChan)
    : mCommChannel(commChan)
    , mPort(port)
{
}

aos::Error CommunicationChannel::Connect()
{
    std::unique_lock<std::mutex> lock(mCommChannelMutex);

    LOG_DBG() << "Connect in communication channel";

    return mCommChannel->Connect();
}

aos::Error CommunicationChannel::Read(std::vector<uint8_t>& message)
{
    LOG_DBG() << "Requesting: port=" << mPort << " size=" << message.size();

    std::unique_lock<std::mutex> lock(mMutex);
    mCondVar.wait(lock, [this] { return !mReceivedMessage.empty() || mShutdown; });

    if (mShutdown) {
        return aos::ErrorEnum::eRuntime;
    }

    if (mReceivedMessage.size() < message.size()) {
        return aos::ErrorEnum::eRuntime;
    }

    message.assign(mReceivedMessage.begin(), mReceivedMessage.begin() + message.size());
    mReceivedMessage.erase(mReceivedMessage.begin(), mReceivedMessage.begin() + message.size());

    return aos::ErrorEnum::eNone;
}

aos::Error CommunicationChannel::Write(std::vector<uint8_t> message)
{
    {
        std::unique_lock<std::mutex> lock(mMutex);
        if (mShutdown) {
            return aos::ErrorEnum::eRuntime;
        }
    }

    std::unique_lock<std::mutex> lock(mCommChannelMutex);

    LOG_DBG() << "Write data: port=" << mPort << " size=" << message.size();

    auto header = PrepareHeader(mPort, message);
    if (header.empty()) {
        return aos::Error(aos::ErrorEnum::eRuntime, "failed to prepare header");
    }

    if (auto err = mCommChannel->Write(std::move(header)); !err.IsNone()) {
        return err;
    }

    LOG_DBG() << "Write message: msg=" << message.size();

    return mCommChannel->Write(std::move(message));
}

aos::Error CommunicationChannel::Close()
{
    LOG_DBG() << "Close communication channel: port=" << mPort;

    {
        std::unique_lock<std::mutex> lock(mMutex);
        if (mShutdown) {
            return aos::ErrorEnum::eFailed;
        }

        mShutdown = true;
    }

    mCondVar.notify_all();

    std::unique_lock<std::mutex> lock(mCommChannelMutex);

    return mCommChannel->Close();
}

aos::Error CommunicationChannel::Receive(std::vector<uint8_t> message)
{
    std::unique_lock<std::mutex> lock(mMutex);

    LOG_DBG() << "Received message: port=" << mPort << " size=" << message.size();

    mReceivedMessage.insert(mReceivedMessage.end(), message.begin(), message.end());
    mCondVar.notify_all();

    LOG_DBG() << "Buffer: size=" << mReceivedMessage.size();

    return aos::ErrorEnum::eNone;
}
