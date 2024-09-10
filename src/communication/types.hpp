#ifndef COMMUNICATION_TYPES_HPP
#define COMMUNICATION_TYPES_HPP

#include <cstdint>
#include <memory>
#include <vector>

#include <aos/common/tools/error.hpp>

#include "iamclient/types.hpp"

class CommChannelItf {
public:
    ~CommChannelItf() = default;

    virtual aos::Error Connect() = 0;

    /**
     * Read message.
     *
     * @param message Message.
     * @return aos::Error.
     */
    virtual aos::Error Read(std::vector<uint8_t>& message) = 0;

    /**
     * Write message.
     *
     * @param message Message.
     * @return aos::Error.
     */
    virtual aos::Error Write(std::vector<uint8_t> message) = 0;

    /**
     * Close channel.
     *
     * @return aos::Error.
     */
    virtual aos::Error Close() = 0;
};

class TransportItf {
public:
    ~TransportItf() = default;

    virtual aos::Error Connect() = 0;

    /**
     * Read message.
     *
     * @param message Message.
     * @return aos::Error.
     */
    virtual aos::Error Read(std::vector<uint8_t>& message) = 0;

    /**
     * Write message.
     *
     * @return aos::Error.
     */
    virtual aos::Error Write(std::vector<uint8_t> message) = 0;

    /**
     * Close channel.
     *
     * @return aos::Error.
     */
    virtual aos::Error Close() = 0;
};

class CommunicationManagerItf : public CommChannelItf {
public:
    /**
     * Create channel.
     *
     * @param port Port.
     * @param certProvider Certificate provider.
     * @param certStorage Certificate storage.
     * @return std::unique_ptr<CommChannelItf>.
     */
    virtual std::unique_ptr<CommChannelItf> CreateChannel(
        int port, CertProviderItf* certProvider = nullptr, const std::string& certStorage = "")
        = 0;
};

class HandlerItf {
public:
    /**
     * Destructor.
     */
    virtual ~HandlerItf() = default;

    /**
     * Notify about connection.
     */
    virtual void OnConnected() = 0;

    /**
     * Notify about disconnection.
     */
    virtual void OnDisconnected() = 0;

    /**
     * Send messages.
     *
     * @param messages Messages.
     * @return aos::Error.
     */
    virtual aos::Error SendMessages(std::vector<uint8_t> messages) = 0;

    /**
     * Receive messages.
     *
     * @return aos::RetWithError<std::vector<uint8_t>>.
     */
    virtual aos::RetWithError<std::vector<uint8_t>> ReceiveMessages() = 0;
};

#endif // COMMUNICATION_TYPES_HPP
