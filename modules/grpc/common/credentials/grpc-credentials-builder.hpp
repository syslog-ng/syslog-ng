/*
 * Copyright (c) 2023 Attila Szakacs
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#ifndef GRPC_CREDENTIALS_BUILDER_HPP
#define GRPC_CREDENTIALS_BUILDER_HPP

#include <grpcpp/server.h>
#include <grpcpp/security/credentials.h>
#include "grpc-credentials-builder.h"

namespace syslogng {
namespace grpc {

class ServerCredentialsBuilder
{
public:
  using ServerAuthMode = GrpcServerAuthMode;
  using ServerTlsPeerVerify = GrpcServerTlsPeerVerify;

  void set_mode(ServerAuthMode mode);
  bool validate() const;
  std::shared_ptr<::grpc::ServerCredentials> build() const;

  /* TLS */
  bool set_tls_ca_path(const char *ca_path);
  bool set_tls_key_path(const char *key_path);
  bool set_tls_cert_path(const char *cert_path);
  void set_tls_peer_verify(ServerTlsPeerVerify peer_verify);

private:
  ServerAuthMode mode = GSAM_INSECURE;

  /* TLS */
  ::grpc::SslServerCredentialsOptions ssl_server_credentials_options { GRPC_SSL_REQUEST_AND_REQUIRE_CLIENT_CERTIFICATE_AND_VERIFY };

  /* ALTS */
  ::grpc::experimental::AltsServerCredentialsOptions alts_server_credentials_options;
};

class ClientCredentialsBuilder
{
public:
  using ClientAuthMode = GrpcClientAuthMode;

  void set_mode(ClientAuthMode mode);
  bool validate() const;
  std::shared_ptr<::grpc::ChannelCredentials> build() const;

  /* TLS */
  bool set_tls_ca_path(const char *ca_path);
  bool set_tls_key_path(const char *key_path);
  bool set_tls_cert_path(const char *cert_path);

  /* ALTS */
  void add_alts_target_service_account(const char *target_service_account);

  /*SERVICE ACCOUNTS*/
  bool set_service_account_key_path(const char *key_path);
  void set_service_account_validity_duration(guint64 validity_duration);

private:
  ClientAuthMode mode = GCAM_INSECURE;

  /* TLS */
  ::grpc::SslCredentialsOptions ssl_credentials_options;

  /* ALTS */
  ::grpc::experimental::AltsCredentialsOptions alts_credentials_options;

  /* SERVICE ACCOUNT */
  struct
  {
    std::string key;
    guint64 validity_duration = 3600L;
  } service_account;
};

}
}

struct GrpcServerCredentialsBuilderW_
{
  syslogng::grpc::ServerCredentialsBuilder *self;
};

struct GrpcClientCredentialsBuilderW_
{
  syslogng::grpc::ClientCredentialsBuilder *self;
};

#endif
