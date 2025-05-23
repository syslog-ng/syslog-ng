/*
 * Copyright (c) 2025 Axoflow
 * Copyright (c) 2025 Tamas Kosztyu <tamas.kosztyu@axoflow.com>
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

#ifndef AZURE_AUTH_HPP
#define AZURE_AUTH_HPP

#include "azure-auth.h"
#include "cloud-auth.hpp"

#include <mutex>
#include <jwt-cpp/jwt.h>

namespace syslogng {
namespace cloud_auth {
namespace azure {

class AzureMonitorAuthenticator: public syslogng::cloud_auth::Authenticator
{
public:
  AzureMonitorAuthenticator(const char *tenant_id, const char *app_id,
                            const char *app_secret, const char *scope);
  ~AzureMonitorAuthenticator() {};

  void handle_http_header_request(HttpHeaderRequestSignalData *data);

private:
  std::string auth_url;
  std::string auth_body;

  std::mutex lock;
  std::string cached_token;
  std::chrono::system_clock::time_point refresh_token_after;

  void add_token_to_header(HttpHeaderRequestSignalData *data);
  bool parse_token_and_expiry_from_response(const std::string &response_payload,
                                            std::string &token, long *expiry);
  static size_t curl_write_callback(void *contents, size_t size, size_t nmemb, void *userp);
  bool send_token_post_request(std::string &response_payload_buffer);
};

}
}
}

#endif
