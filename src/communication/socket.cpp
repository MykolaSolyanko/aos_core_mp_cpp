/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "socket.hpp"
#include "logger/logmodule.hpp"
#include <Poco/Net/SocketAddress.h>

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

aos::Error Socket::Init(int port)
{
    LOG_DBG() << "Initializing socket with: port=" << port;

    mPort = port;

    try {
        mServerSocket.bind(Poco::Net::SocketAddress("0.0.0.0", mPort), true, true);
        mServerSocket.listen(1);

        mReactor.addEventHandler(
            mServerSocket, Poco::Observer<Socket, Poco::Net::ReadableNotification>(*this, &Socket::OnAccept));

        mReactorThread = std::thread(&Socket::ReactorThread, this);

        LOG_DBG() << "Socket initialized and listening on: port=" << mPort;
    } catch (const Poco::Exception& e) {
        return aos::Error {aos::ErrorEnum::eRuntime, e.displayText().c_str()};
    }

    return aos::ErrorEnum::eNone;
}

aos::Error Socket::Connect()
{
    if (mShutdown) {
        return aos::ErrorEnum::eFailed;
    }

    LOG_DBG() << "Waiting for client connection";

    std::unique_lock<std::mutex> lock(mMutex);
    mCV.wait(lock, [this] { return mConnectionAccepted || mShutdown; });
    mConnectionAccepted = false;

    if (mShutdown) {
        return aos::Error {EINTR};
    }

    return aos::ErrorEnum::eNone;
}

aos::Error Socket::Close()
{
    LOG_DBG() << "Closing the current connection";

    if (mShutdown) {
        return aos::ErrorEnum::eNone;
    }

    mShutdown = true;
    mReactor.stop();

    if (mReactorThread.joinable()) {
        mReactorThread.join();
    }

    mReactor.removeEventHandler(
        mServerSocket, Poco::Observer<Socket, Poco::Net::ReadableNotification>(*this, &Socket::OnAccept));

    try {
        if (mClientSocket.impl()->initialized()) {
            mClientSocket.shutdown();
            mClientSocket.close();
        }

        mServerSocket.close();
    } catch (const Poco::Exception& e) {
        return aos::Error {aos::ErrorEnum::eRuntime, e.displayText().c_str()};
    }

    mCV.notify_all();

    return aos::ErrorEnum::eNone;
}

aos::Error Socket::Read(std::vector<uint8_t>& message)
{
    LOG_DBG() << "Read from the client, expected: size=" << message.size();

    try {
        int totalRead = 0;
        while (totalRead < static_cast<int>(message.size())) {
            int bytesRead = mClientSocket.receiveBytes(message.data() + totalRead, message.size() - totalRead);
            if (bytesRead == 0) {
                return aos::Error {ECONNRESET};
            }

            totalRead += bytesRead;
        }

        LOG_DBG() << "Total read: totalRead=" << totalRead << " size=" << message.size();
    } catch (const Poco::Exception& e) {
        return aos::Error {aos::ErrorEnum::eRuntime, e.displayText().c_str()};
    }

    return aos::ErrorEnum::eNone;
}

aos::Error Socket::Write(std::vector<uint8_t> message)
{
    LOG_DBG() << "Write to the client: size=" << message.size();

    try {
        int totalSent = 0;
        while (totalSent < static_cast<int>(message.size())) {
            int bytesSent = mClientSocket.sendBytes(message.data() + totalSent, message.size() - totalSent);
            if (bytesSent == 0) {
                return aos::Error {ECONNRESET};
            }

            totalSent += bytesSent;
        }

        LOG_DBG() << "Total written: totalSent=" << totalSent << " size=" << message.size();
    } catch (const Poco::Exception& e) {
        return aos::Error {aos::ErrorEnum::eRuntime, e.displayText().c_str()};
    }

    return aos::ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

void Socket::ReactorThread()
{
    while (!mShutdown) {
        try {
            mReactor.run();
        } catch (const Poco::Exception& e) {
            if (!mShutdown) {
                LOG_ERR() << "Reactor: error=" << e.displayText().c_str();
            }
        }
    }
}

void Socket::OnAccept([[maybe_unused]] Poco::Net::ReadableNotification* pNf)
{
    try {
        mClientSocket = mServerSocket.acceptConnection();

        LOG_DBG() << "Client connected: address=" << mClientSocket.peerAddress().toString().c_str();

        {
            std::lock_guard<std::mutex> lock(mMutex);
            mConnectionAccepted = true;
        }

        mCV.notify_all();
    } catch (const Poco::Exception& e) {
        LOG_ERR() << "Failed to accept connection: error=" << e.displayText().c_str();
    }
}
