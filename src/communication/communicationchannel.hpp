/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef COMMUNICATIONCHANNEL_HPP_
#define COMMUNICATIONCHANNEL_HPP_

#include <condition_variable>
#include <mutex>
#include <vector>

#include "types.hpp"

/**
 * Communication channel class
 */
class CommunicationChannel : public CommChannelItf {
public:
    /**
     * Constructor.
     *
     * @param port Port
     * @param commChan Communication channel
     */
    CommunicationChannel(int port, CommChannelItf* commChan);

    /**
     * Destructor.
     */
    aos::Error Connect() override;

    /**
     * Read message from the communication channel.
     *
     * @param message Message
     * @return aos::Error
     */
    aos::Error Read(std::vector<uint8_t>& message) override;

    /**
     * Write message to the communication channel.
     *
     * @param message Message
     * @return aos::Error
     */
    aos::Error Write(std::vector<uint8_t> message) override;

    /**
     * Close the communication channel.
     *
     * @return aos::Error
     */
    aos::Error Close() override;

    /**
     * Receive message.
     *
     * @param message Message
     * @return aos::Error
     */
    aos::Error Receive(std::vector<uint8_t> message);

private:
    // Global mutex for synchronization communication channel
    static std::mutex mCommChannelMutex;

    CommChannelItf*         mCommChannel {};
    int                     mPort {-1};
    bool                    mShutdown {false};
    std::vector<uint8_t>    mReceivedMessage;
    std::mutex              mMutex;
    std::condition_variable mCondVar;
};

#endif /* COMMUNICATIONCHANNEL_HPP_ */
