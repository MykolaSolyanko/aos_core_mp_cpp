/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "vchan.hpp"
#include "logger/logmodule.hpp"

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

aos::Error VChan::Init(const VChanConfig& config)
{
    LOG_DBG() << "Initialize the virtual channel";

    mConfig = config;

    return aos::ErrorEnum::eNone;
}

aos::Error VChan::Connect()
{
    if (mShutdown) {
        return aos::ErrorEnum::eFailed;
    }

    LOG_DBG() << "Connect to the virtual channel";

    if (auto error = ConnectToVChan(mVChanRead, mConfig.mXSRXPath, mConfig.mDomain); !error.IsNone()) {
        return error;
    }

    return ConnectToVChan(mVChanWrite, mConfig.mXSTXPath, mConfig.mDomain);
}

aos::Error VChan::Read(std::vector<uint8_t>& message)
{
    LOG_DBG() << "Read from the virtual channel, size=" << message.size();

    int read {};

    while (read < static_cast<int>(message.size())) {
        int len = libxenvchan_read(mVChanRead, message.data() + read, message.size() - read);
        if (len < 0) {
            return len;
        }

        read += len;
    }

    return aos::ErrorEnum::eNone;
}

aos::Error VChan::Write(std::vector<uint8_t> message)
{
    LOG_DBG() << "Write to the virtual channel, size=" << message.size();

    int written {};

    while (written < static_cast<int>(message.size())) {
        int len = libxenvchan_write(mVChanWrite, message.data() + written, message.size() - written);
        if (len < 0) {
            return len;
        }

        written += len;
    }

    return aos::ErrorEnum::eNone;
}

aos::Error VChan::Close()
{
    LOG_DBG() << "Close the virtual channel";

    libxenvchan_close(mVChanRead);
    libxenvchan_close(mVChanWrite);

    mShutdown = true;

    return aos::ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

aos::Error VChan::ConnectToVChan(struct libxenvchan*& vchan, const std::string& path, int domain)
{
    vchan = libxenvchan_server_init(nullptr, domain, path.c_str(), 0, 0);
    if (vchan == nullptr) {
        return aos::Error(aos::ErrorEnum::eFailed, errno != 0 ? strerror(errno) : "failed to connect");
    }

    vchan->blocking = 0x1;

    return aos::ErrorEnum::eNone;
}