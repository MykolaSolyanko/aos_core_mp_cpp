#include "openchannel.hpp"
#include "logger/logmodule.hpp"

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

OpenChannel::OpenChannel(CommChannelItf* channel, int port)
    : mChannel(channel)
    , mPort(port)
{
}

aos::Error OpenChannel::Connect()
{
    LOG_DBG() << "Connect to the open channel port=" << mPort;

    return mChannel->Connect();
}

aos::Error OpenChannel::Read(std::vector<uint8_t>& message)
{
    return mChannel->Read(message);
}

aos::Error OpenChannel::Write(std::vector<uint8_t> message)
{
    return mChannel->Write(std::move(message));
}

aos::Error OpenChannel::Close()
{
    LOG_DBG() << "Close channel port=" << mPort;

    return mChannel->Close();
}
