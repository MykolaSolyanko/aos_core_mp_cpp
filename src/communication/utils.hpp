#ifndef UTILS_HPP
#define UTILS_HPP

#include <cstring>
#include <vector>

#include <aosprotocol.h>

#include <aos/common/tools/error.hpp>
#include <openssl/sha.h>

constexpr size_t cProtobufHeaderSize = sizeof(AosProtobufHeader);
constexpr size_t cHeaderSize         = sizeof(AosProtocolHeader);

/**
 * Prepare header
 *
 * @param port Port
 * @param data Data
 * @return Header
 */
inline std::vector<uint8_t> PrepareHeader(uint32_t port, const std::vector<uint8_t>& data)
{
    AosProtocolHeader header {};

    header.mPort     = port;
    header.mDataSize = static_cast<uint32_t>(data.size());

    SHA256(data.data(), data.size(), header.mCheckSum);

    std::vector<uint8_t> headerVector(cHeaderSize);
    std::memcpy(headerVector.data(), &header, cHeaderSize);

    return headerVector;
}

/**
 * Prepare protobuf header
 *
 * @param dataSize Data size
 * @return Header
 */
inline std::vector<uint8_t> PrepareProtobufHeader(uint32_t dataSize)
{
    AosProtobufHeader header {};

    header.mDataSize = dataSize;

    std::vector<uint8_t> headerVector(cProtobufHeaderSize);
    std::memcpy(headerVector.data(), &header, cProtobufHeaderSize);

    return headerVector;
}

/**
 * Parse protobuf header
 *
 * @param header Header
 * @return Header struct
 */
inline AosProtobufHeader ParseProtobufHeader(const std::vector<uint8_t>& header)
{
    AosProtobufHeader headerStruct {};

    std::memcpy(&headerStruct, header.data(), cProtobufHeaderSize);

    return headerStruct;
}

#endif // UTILS_HPP
