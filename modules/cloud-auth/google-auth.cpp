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

#include "google-auth.hpp"

#include "compat/cpp-start.h"
#include "scratch-buffers.h"
#include "compat/cpp-end.h"

#include <exception>
#include <fstream>
#include <cmath>

using namespace syslogng::cloud_auth::google;

ServiceAccountAuthenticator::ServiceAccountAuthenticator(const char *key_path, const char *audience_,
                                                         uint64_t token_validity_duration_)
  : token_validity_duration(token_validity_duration_)
{
  picojson::value key_json;

  try
    {
      std::ifstream key_file(key_path);
      std::string parse_error = picojson::parse(key_json, key_file);
      key_file.close();

      if (!parse_error.empty())
        throw std::runtime_error(std::string("Failed to parse key file: ") + parse_error);
    }
  catch (const std::ifstream::failure &e)
    {
      throw std::runtime_error(std::string("Failed to open key file: ") + e.what());
    }

  try
    {
      email = key_json.get("client_email").get<std::string>();
      private_key_id = key_json.get("private_key_id").get<std::string>();
      private_key = key_json.get("private_key").get<std::string>();
    }
  catch (const std::runtime_error &e)
    {
      throw std::runtime_error(std::string("Failed to get the necessary fields from the key file: ") + e.what());
    }

  if (audience_ == nullptr)
    throw std::runtime_error(std::string("audience() is mandatory"));
  audience = audience_;
}

void
ServiceAccountAuthenticator::handle_http_header_request(HttpHeaderRequestSignalData *data)
{
  /* Scratch Buffers are marked at this point in http-worker.c */
  GString *buffer = scratch_buffers_alloc();
  g_string_append(buffer, "Authorization: Bearer ");

  std::chrono::system_clock::time_point issued_at = jwt::date::clock::now();
  std::chrono::system_clock::time_point expires_at = issued_at + std::chrono::seconds{token_validity_duration};

  try
    {
      jwt::traits::kazuho_picojson::string_type token = jwt::create()
                                                        .set_issuer(email)
                                                        .set_subject(email)
                                                        .set_audience(audience)
                                                        .set_key_id(private_key_id)
                                                        .set_issued_at(issued_at)
                                                        .set_expires_at(expires_at)
                                                        .sign(jwt::algorithm::rs256("", private_key, "", ""));
      g_string_append(buffer, token.c_str());
    }
  catch (const std::system_error &e)
    {
      msg_error("cloud_auth::google::ServiceAccountAuthenticator: Failed to generate JWT token",
                evt_tag_str("error", e.what()));
      data->result = HTTP_SLOT_CRITICAL_ERROR;
      return;
    }

  list_append(data->request_headers, buffer->str);
  data->result = HTTP_SLOT_SUCCESS;
}

UserManagedServiceAccountAuthenticator::UserManagedServiceAccountAuthenticator(const char *name_,
    const char *metadata_url_)
  : name(name_)
{
  auth_url = metadata_url_;
  auth_url.append("/");
  auth_url.append(name);
  auth_url.append("/token");

  curl_headers = curl_slist_append(NULL, "Metadata-Flavor: Google");
}

UserManagedServiceAccountAuthenticator::~UserManagedServiceAccountAuthenticator()
{
  curl_slist_free_all(curl_headers);
}

void
UserManagedServiceAccountAuthenticator::add_token_to_headers(HttpHeaderRequestSignalData *data,
    const std::string &token)
{
  /* Scratch Buffers are marked at this point in http-worker.c */
  GString *header_buffer = scratch_buffers_alloc();
  g_string_append(header_buffer, "Authorization: Bearer ");
  g_string_append(header_buffer, token.c_str());

  list_append(data->request_headers, header_buffer->str);
}

size_t
UserManagedServiceAccountAuthenticator::curl_write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
  const char *data = (const char *) contents;
  std::string *response_payload_buffer = (std::string *) userp;

  size_t real_size = size * nmemb;
  response_payload_buffer->append(data, real_size);

  return real_size;
}

bool
UserManagedServiceAccountAuthenticator::send_token_get_request(std::string &response_payload_buffer)
{
  CURLcode res;
  CURL *curl = curl_easy_init();
  if (!curl)
    {
      msg_error("cloud_auth::google::UserManagedServiceAccountAuthenticator: "
                "failed to init cURL handle",
                evt_tag_str("url", auth_url.c_str()));
      goto error;
    }

  curl_easy_setopt(curl, CURLOPT_URL, auth_url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &response_payload_buffer);

  res = curl_easy_perform(curl);
  if (res != CURLE_OK)
    {
      msg_error("cloud_auth::google::UserManagedServiceAccountAuthenticator: "
                "error sending HTTP request to metadata server",
                evt_tag_str("url", auth_url.c_str()),
                evt_tag_str("error", curl_easy_strerror(res)));
      goto error;
    }

  long http_result_code;
  res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_result_code);
  if (res != CURLE_OK)
    {
      msg_error("cloud_auth::google::UserManagedServiceAccountAuthenticator: "
                "failed to get HTTP result code",
                evt_tag_str("url", auth_url.c_str()),
                evt_tag_str("error", curl_easy_strerror(res)));
      goto error;
    }

  if (http_result_code != 200)
    {
      msg_error("cloud_auth::google::UserManagedServiceAccountAuthenticator: "
                "non 200 HTTP result code received",
                evt_tag_str("url", auth_url.c_str()),
                evt_tag_int("http_result_code", http_result_code));
      goto error;
    }

  curl_easy_cleanup(curl);
  return true;

error:
  if (curl)
    curl_easy_cleanup(curl);
  return false;
}

bool
UserManagedServiceAccountAuthenticator::parse_token_and_expiry_from_response(const std::string &response_payload,
    std::string &token, long *expiry)
{
  picojson::value json;
  std::string json_parse_error = picojson::parse(json, response_payload);
  if (!json_parse_error.empty())
    {
      msg_error("cloud_auth::google::UserManagedServiceAccountAuthenticator: "
                "failed to parse response JSON",
                evt_tag_str("url", auth_url.c_str()),
                evt_tag_str("response_json", response_payload.c_str()));
      return false;
    }

  if (!json.is<picojson::object>() || !json.contains("access_token") || !json.contains("expires_in"))
    {
      msg_error("cloud_auth::google::UserManagedServiceAccountAuthenticator: "
                "unexpected response JSON",
                evt_tag_str("url", auth_url.c_str()),
                evt_tag_str("response_json", response_payload.c_str()));
      return false;
    }

  token.assign(json.get("access_token").get<std::string>());
  *expiry = lround(json.get("expires_in").get<double>()); /* getting a long from picojson is not always available */
  return true;
}

void
UserManagedServiceAccountAuthenticator::handle_http_header_request(HttpHeaderRequestSignalData *data)
{
  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

  lock.lock();

  if (now <= refresh_token_after && !cached_token.empty())
    {
      add_token_to_headers(data, cached_token);
      lock.unlock();

      data->result = HTTP_SLOT_SUCCESS;
      return;
    }

  cached_token.clear();

  std::string response_payload_buffer;
  if (!send_token_get_request(response_payload_buffer))
    {
      lock.unlock();

      data->result = HTTP_SLOT_CRITICAL_ERROR;
      return;
    }

  long expiry;
  if (!parse_token_and_expiry_from_response(response_payload_buffer, cached_token, &expiry))
    {
      lock.unlock();

      data->result = HTTP_SLOT_CRITICAL_ERROR;
      return;
    }

  refresh_token_after = now + std::chrono::seconds{expiry - 60};
  add_token_to_headers(data, cached_token);

  lock.unlock();

  data->result = HTTP_SLOT_SUCCESS;
}

/* C Wrappers */

typedef struct _GoogleAuthenticator
{
  CloudAuthenticator super;
  GoogleAuthenticatorAuthMode auth_mode;

  struct
  {
    gchar *key_path;
    gchar *audience;
    guint64 token_validity_duration;
  } service_account_options;

  struct
  {
    gchar *name;
    gchar *metadata_url;
  } user_managed_service_account_options;

} GoogleAuthenticator;

void
google_authenticator_set_auth_mode(CloudAuthenticator *s, GoogleAuthenticatorAuthMode auth_mode)
{
  GoogleAuthenticator *self = (GoogleAuthenticator *) s;

  self->auth_mode = auth_mode;
}

void
google_authenticator_set_service_account_key_path(CloudAuthenticator *s, const gchar *key_path)
{
  GoogleAuthenticator *self = (GoogleAuthenticator *) s;

  g_free(self->service_account_options.key_path);
  self->service_account_options.key_path = g_strdup(key_path);
}

void
google_authenticator_set_service_account_audience(CloudAuthenticator *s, const gchar *audience)
{
  GoogleAuthenticator *self = (GoogleAuthenticator *) s;

  g_free(self->service_account_options.audience);
  self->service_account_options.audience = g_strdup(audience);
}

void
google_authenticator_set_service_account_token_validity_duration(CloudAuthenticator *s, guint64 token_validity_duration)
{
  GoogleAuthenticator *self = (GoogleAuthenticator *) s;

  self->service_account_options.token_validity_duration = token_validity_duration;
}

void
google_authenticator_set_user_managed_service_account_name(CloudAuthenticator *s, const gchar *name)
{
  GoogleAuthenticator *self = (GoogleAuthenticator *) s;

  g_free(self->user_managed_service_account_options.name);
  self->user_managed_service_account_options.name = g_strdup(name);
}

void
google_authenticator_set_user_managed_service_account_metadata_url(CloudAuthenticator *s, const gchar *metadata_url)
{
  GoogleAuthenticator *self = (GoogleAuthenticator *) s;

  g_free(self->user_managed_service_account_options.metadata_url);
  self->user_managed_service_account_options.metadata_url = g_strdup(metadata_url);
}

static gboolean
_init(CloudAuthenticator *s)
{
  GoogleAuthenticator *self = (GoogleAuthenticator *) s;

  switch (self->auth_mode)
    {
    case GAAM_SERVICE_ACCOUNT:
      try
        {
          self->super.cpp = new ServiceAccountAuthenticator(self->service_account_options.key_path,
                                                            self->service_account_options.audience,
                                                            self->service_account_options.token_validity_duration);
        }
      catch (const std::runtime_error &e)
        {
          msg_error("cloud_auth::google: Failed to initialize ServiceAccountAuthenticator",
                    evt_tag_str("error", e.what()));
          return FALSE;
        }
      break;
    case GAAM_USER_MANAGED_SERVICE_ACCOUNT:
      try
        {
          self->super.cpp = new UserManagedServiceAccountAuthenticator(self->user_managed_service_account_options.name,
              self->user_managed_service_account_options.metadata_url);
        }
      catch (const std::runtime_error &e)
        {
          msg_error("cloud_auth::google: Failed to initialize UserManagedServiceAccountAuthenticator",
                    evt_tag_str("error", e.what()));
          return FALSE;
        }
      break;
    case GAAM_UNDEFINED:
      msg_error("cloud_auth::google: Failed to initialize ServiceAccountAuthenticator",
                evt_tag_str("error", "Authentication mode must be set (e.g. service-account())"));
      return FALSE;
    default:
      g_assert_not_reached();
    }

  return TRUE;
}

static void
_set_default_options(GoogleAuthenticator *self)
{
  self->service_account_options.token_validity_duration = 3600;

  self->user_managed_service_account_options.name = g_strdup("default");
  self->user_managed_service_account_options.metadata_url =
    g_strdup("http://metadata.google.internal/computeMetadata/v1/instance/service-accounts");
}

static void
_free(CloudAuthenticator *s)
{
  GoogleAuthenticator *self = (GoogleAuthenticator *) s;

  g_free(self->service_account_options.key_path);
  g_free(self->service_account_options.audience);

  g_free(self->user_managed_service_account_options.name);
  g_free(self->user_managed_service_account_options.metadata_url);
}

CloudAuthenticator *
google_authenticator_new(void)
{
  GoogleAuthenticator *self = g_new0(GoogleAuthenticator, 1);

  self->super.init = _init;
  self->super.free_fn = _free;

  _set_default_options(self);

  return &self->super;
}
