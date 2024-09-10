/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef VCHAN_HPP_
#define VCHAN_HPP_

extern "C" {
#include <libxenvchan.h>
}

#include <atomic>

#include "config/config.hpp"
#include "types.hpp"

/**
 * Virtual Channel class
 */
class VChan : public TransportItf {
public:
    aos::Error Init(const VChanConfig& config);

    /**
     * Connect to the virtual channel
     */
    aos::Error Connect() override;

    /**
     * Read message from the virtual channel
     *
     * @param message Message
     * @return aos::Error
     */
    aos::Error Read(std::vector<uint8_t>& message) override;

    /**
     * Write message to the virtual channel
     *
     * @param message Message
     * @return aos::Error
     */
    aos::Error Write(std::vector<uint8_t> message) override;

    /**
     * Close the virtual channel
     *
     * @return aos::Error
     */
    aos::Error Close() override;

private:
    aos::Error ConnectToVChan(struct libxenvchan*& vchan, const std::string& path, int domain);

    struct libxenvchan* mVChanRead {};
    struct libxenvchan* mVChanWrite {};
    VChanConfig         mConfig;
    std::atomic<bool>   mShutdown {false};
};

#endif // VCHAN_HPP_
