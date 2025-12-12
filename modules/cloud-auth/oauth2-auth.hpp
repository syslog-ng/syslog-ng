/*
 * Copyright (c) 2025 Databricks
 * Copyright (c) 2025 David Tosovic <david.tosovic@databricks.com>
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

#ifndef OAUTH2_AUTH_HPP
#define OAUTH2_AUTH_HPP

#include "oauth2-auth.h"
#include "cloud-auth.hpp"

#include <mutex>
#include <string>
#include <jwt-cpp/jwt.h>
#include "compat/curl.h"

namespace syslogng {
namespace cloud_auth {
namespace oauth2 {

class OAuth2Authenticator: public syslogng::cloud_auth::Authenticator
{
public:
  OAuth2Authenticator(
    const char *client_id,
    const char *client_secret,
    const char *token_url,
    const char *scope,
    const char *resource,
    const char *authorization_details,
    uint64_t refresh_offset_seconds,
    OAuth2AuthMethod auth_method
  );

  void handle_http_header_request(HttpHeaderRequestSignalData *data);
  void handle_grpc_metadata_request(GrpcMetadataRequestSignalData *data);

protected:
  std::string client_id;
  std::string client_secret;
  std::string token_url;
  std::string scope;
  std::string resource;
  std::string authorization_details;
  uint64_t refresh_offset_seconds;
  OAuth2AuthMethod auth_method;

  std::mutex lock;
  std::string cached_token;
  std::chrono::system_clock::time_point refresh_token_after;

  void add_token_to_header(HttpHeaderRequestSignalData *data);
  void add_token_to_grpc_metadata(GrpcMetadataRequestSignalData *data);
  bool send_token_post_request(std::string &response_payload_buffer);
  bool extract_token_from_response(const std::string &response);
  std::string build_post_body();
  std::string url_encode(CURL *hnd, const std::string &value);

  // Virtual method for subclasses to override auth credential preparation
  virtual void prepare_auth_credentials(CURL *hnd, std::string &post_body);

  static size_t curl_write_callback(void *ptr, size_t size, size_t nmemb, void *userdata);

private:
  template<typename SignalDataT, typename ResultT, typename AddTokenFn>
  void handle_token_request_impl(SignalDataT *data, AddTokenFn add_token_fn,
                                 ResultT success_code, ResultT error_code);
};

}
}
}

#endif

