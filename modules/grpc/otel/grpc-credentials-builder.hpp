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

}
}

struct GrpcServerCredentialsBuilderW_
{
  syslogng::grpc::ServerCredentialsBuilder *self;
};

#endif
