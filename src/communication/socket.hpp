/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SOCKET_HPP_
#define SOCKET_HPP_

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/SocketNotification.h>
#include <Poco/Net/SocketReactor.h>
#include <Poco/Net/StreamSocket.h>

#include "types.hpp"

/**
 * Socket class
 */
class Socket : public TransportItf {
public:
    /**
     * Initialize the socket
     *
     * @param port Port
     * @return aos::Error
     */
    aos::Error Init(int port);

    /**
     * Connect to the socket
     *
     * @return aos::Error
     */
    aos::Error Connect() override;

    /**
     * Read message from the socket
     *
     * @param message Message
     * @return aos::Error
     */
    aos::Error Read(std::vector<uint8_t>& message) override;

    /**
     * Write message to the socket
     *
     * @param message Message
     * @return aos::Error
     */
    aos::Error Write(std::vector<uint8_t> message) override;

    /**
     * Close the socket
     *
     * @return aos::Error
     */
    aos::Error Close() override;

private:
    void OnAccept(Poco::Net::ReadableNotification* pNf);
    void CleanupResources();
    void ReactorThread();

    int                      mPort {-1};
    std::atomic<bool>        mShutdown {};
    bool                     mConnectionAccepted {};
    Poco::Net::ServerSocket  mServerSocket;
    Poco::Net::StreamSocket  mClientSocket;
    Poco::Net::SocketReactor mReactor;
    std::thread              mReactorThread;
    std::mutex               mMutex;
    std::condition_variable  mCV;
};

#endif // SOCKET_HPP_
