/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IAMCLIENT_HPP_
#define IAMCLIENT_HPP_

#include <memory>
#include <optional>
#include <string>

#include <utils/grpchelper.hpp>

#include "config/config.hpp"
#include "publicnodeclient.hpp"
#include "publicservicehandler.hpp"
#include "types.hpp"

/**
 * IAM client.
 */
class IAMClient : public CertProviderItf {
public:
    IAMClient() = default;
    /**
     * Initialize IAM client.
     *
     * @param cfg Configuration.
     * @param certLoader Certificate loader.
     * @param cryptoProvider Crypto provider.
     * @param channel Channel.
     * @param provisioningMode Provisioning mode.
     * @param mtlsCredentialsFunc MTLS credentials function.
     * @return aos::Error.
     */
    aos::Error Init(const Config& cfg, aos::cryptoutils::CertLoaderItf& certLoader,
        aos::crypto::x509::ProviderItf& cryptoProvider, bool provisioningMode = false,
        MTLSCredentialsFunc mtlsCredentialsFunc = aos::common::utils::GetMTLSClientCredentials);

    /**
     * Get MTLS configuration.
     *
     * @param certStorage Certificate storage.
     * @return MTLS configuration.
     */
    aos::RetWithError<std::shared_ptr<grpc::ChannelCredentials>> GetMTLSConfig(const std::string& certStorage) override;

    /**
     * Get TLS credentials.
     *
     * @return TLS credentials.
     */
    std::shared_ptr<grpc::ChannelCredentials> GetTLSCredentials() override;

    /**
     * Get certificate.
     *
     * @param certType Certificate type.
     * @param certInfo Certificate information.
     * @return aos::Error.
     */
    aos::Error GetCertificate(const std::string& certType, aos::iam::certhandler::CertInfo& certInfo) override;

    /**
     * Get public handler.
     *
     * @return Public handler.
     */
    HandlerItf& GetPublicHandler() { return mPublicNodeClient.value(); }

    /**
     * Get protected handler.
     *
     * @return Protected handler.
     */
    HandlerItf& GetProtectedHandler() { return mProtectedNodeClient.value(); }

private:
    std::optional<PublicServiceHandler> mPublicServiceHandler;
    std::optional<PublicNodeClient>     mPublicNodeClient;
    std::optional<PublicNodeClient>     mProtectedNodeClient;
};

#endif
