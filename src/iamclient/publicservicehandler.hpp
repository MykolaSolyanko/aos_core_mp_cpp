/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PUBLICSERVICEHANDLER_HPP_
#define PUBLICSERVICEHANDLER_HPP_

#include <iamanager/v5/iamanager.grpc.pb.h>

#include "config/config.hpp"
#include "types.hpp"

/**
 * Public service handler.
 */
class PublicServiceHandler {
public:
    /**
     * Initialize handler.
     *
     * @param cfg Configuration.
     * @param certLoader Certificate loader.
     * @param cryptoProvider Crypto provider.
     * @param insecureConnection Insecure connection.
     */
    aos::Error Init(const Config& cfg, aos::cryptoutils::CertLoaderItf& certLoader,
        aos::crypto::x509::ProviderItf& cryptoProvider, bool insecureConnection,
        MTLSCredentialsFunc mtlsCredentialsFunc);

    /**
     * Get MTLS configuration.
     *
     * @param certStorage Certificate storage.
     * @return MTLS configuration.
     */
    aos::RetWithError<std::shared_ptr<grpc::ChannelCredentials>> GetMTLSConfig(const std::string& certStorage);

    /**
     * Get TLS credentials.
     *
     * @return TLS credentials.
     */
    std::shared_ptr<grpc::ChannelCredentials> GetTLSCredentials();

    /**
     * Get certificate.
     *
     * @param certType Certificate type.
     * @param certInfo Certificate info.
     * @return aos::Error.
     */
    aos::Error GetCertificate(const std::string& certType, aos::iam::certhandler::CertInfo& certInfo);

private:
    constexpr static auto cIAMPublicServiceTimeout = std::chrono::seconds(10);

    using IAMPublicServicePtr = std::unique_ptr<iamanager::v5::IAMPublicService::Stub>;

    aos::Error CreateCredentials(bool insecureConnection);

    const Config*                             mConfig;
    aos::cryptoutils::CertLoaderItf*          mCertLoader;
    aos::crypto::x509::ProviderItf*           mCryptoProvider;
    std::shared_ptr<grpc::ChannelCredentials> mCredentials;
    MTLSCredentialsFunc                       mMTLSCredentialsFunc;
};

#endif
