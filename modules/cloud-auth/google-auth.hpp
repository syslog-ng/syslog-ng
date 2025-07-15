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

#ifndef GOOGLE_AUTH_HPP
#define GOOGLE_AUTH_HPP

#include "google-auth.h"
#include "cloud-auth.hpp"

#include <mutex>
#include <string>
#include <jwt-cpp/jwt.h>

#include "compat/curl.h"

namespace syslogng {
namespace cloud_auth {
namespace google {

class ServiceAccountAuthenticator: public syslogng::cloud_auth::Authenticator
{
public:
  ServiceAccountAuthenticator(const char *key_path, const char *audience, uint64_t token_validity_duration);
  ~ServiceAccountAuthenticator() {};

  void handle_http_header_request(HttpHeaderRequestSignalData *data);

private:
  std::string audience;
  std::string email;
  std::string private_key;
  std::string private_key_id;

  uint64_t token_validity_duration;
};

class UserManagedServiceAccountAuthenticator: public syslogng::cloud_auth::Authenticator
{
public:
  UserManagedServiceAccountAuthenticator(const char *name, const char *metadata_url);
  ~UserManagedServiceAccountAuthenticator();

  void handle_http_header_request(HttpHeaderRequestSignalData *data);

private:
  static void add_token_to_headers(HttpHeaderRequestSignalData *data, const std::string &token);
  static size_t curl_write_callback(void *contents, size_t size, size_t nmemb, void *userp);
  bool send_token_get_request(std::string &response_payload_buffer);
  bool parse_token_and_expiry_from_response(const std::string &response_payload, std::string &token, long *expiry);

private:
  std::string name;
  std::string auth_url;

  struct curl_slist *curl_headers;

  std::mutex lock;
  std::string cached_token;
  std::chrono::system_clock::time_point refresh_token_after;
};
}
}
}

#endif
