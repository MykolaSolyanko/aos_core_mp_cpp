/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef OPENCHANNEL_HPP_
#define OPENCHANNEL_HPP_

#include "types.hpp"

/**
 * Open Channel class.
 */
class OpenChannel : public CommChannelItf {
public:
    /**
     * Constructor.
     *
     * @param channel Communication channel.
     * @return aos::Error.
     */
    OpenChannel(CommChannelItf* channel, int port);

    /**
     * Connect to channel.
     *
     * @return aos::Error.
     */
    aos::Error Connect() override;

    /**
     * Read message.
     *
     * @param message Message.
     * @return aos::Error.
     */
    aos::Error Read(std::vector<uint8_t>& message) override;

    /**
     * Write message.
     *
     * @return aos::Error.
     */
    aos::Error Write(std::vector<uint8_t> message) override;

    /**
     * Close channel.
     *
     * @return aos::Error.
     */
    aos::Error Close() override;

private:
    CommChannelItf* mChannel {};
    int             mPort;
};

#endif /* OPENCHANNEL_HPP_ */
