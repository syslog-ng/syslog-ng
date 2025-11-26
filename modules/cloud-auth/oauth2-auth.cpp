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

#include "oauth2-auth.hpp"
#include "compat/cpp-start.h"
#include "messages.h"
#include "scratch-buffers.h"
#include "compat/cpp-end.h"

#include <chrono>
#include <sstream>

using namespace syslogng::cloud_auth;
using namespace syslogng::cloud_auth::oauth2;

OAuth2Authenticator::OAuth2Authenticator(
  const char *client_id_,
  const char *client_secret_,
  const char *token_url_,
  const char *scope_,
  const char *resource_,
  const char *authorization_details_,
  uint64_t refresh_offset_seconds_,
  OAuth2AuthMethod auth_method_
)
  : client_id(client_id_ ? client_id_ : ""),
    client_secret(client_secret_ ? client_secret_ : ""),
    token_url(token_url_ ? token_url_ : ""),
    scope(scope_ ? scope_ : ""),
    resource(resource_ ? resource_ : ""),
    authorization_details(authorization_details_ ? authorization_details_ : ""),
    refresh_offset_seconds(refresh_offset_seconds_),
    auth_method(auth_method_),
    refresh_token_after(std::chrono::system_clock::now())
{
}

void
OAuth2Authenticator::handle_http_header_request(HttpHeaderRequestSignalData *data)
{
  lock.lock();

  if (std::chrono::system_clock::now() <= refresh_token_after && !cached_token.empty())
    {
      add_token_to_header(data);
      lock.unlock();

      data->result = HTTP_SLOT_SUCCESS;
      return;
    }

  cached_token.clear();

  std::string response_payload_buffer;
  if (!send_token_post_request(response_payload_buffer))
    {
      lock.unlock();

      data->result = HTTP_SLOT_CRITICAL_ERROR;
      return;
    }

  if (!extract_token_from_response(response_payload_buffer))
    {
      lock.unlock();

      data->result = HTTP_SLOT_CRITICAL_ERROR;
      return;
    }

  add_token_to_header(data);

  lock.unlock();

  data->result = HTTP_SLOT_SUCCESS;
}

void
OAuth2Authenticator::add_token_to_header(HttpHeaderRequestSignalData *data)
{
  GString *auth_buffer = scratch_buffers_alloc();
  g_string_append(auth_buffer, "Authorization: Bearer ");
  g_string_append(auth_buffer, cached_token.c_str());

  list_append(data->request_headers, auth_buffer->str);
}

std::string
OAuth2Authenticator::build_post_body()
{
  CURL *hnd = curl_easy_init();
  if (!hnd)
    return "";

  std::string post_body = "grant_type=client_credentials";

  // Note: Credentials are NOT added here - see prepare_auth_credentials()
  // which handles credentials based on auth_method (Basic Auth vs POST body)

  // Add scope if provided
  if (!scope.empty())
    {
      post_body += "&scope=" + url_encode(hnd, scope);
    }

  // Add resource if provided
  if (!resource.empty())
    {
      post_body += "&resource=" + url_encode(hnd, resource);
    }

  // Add authorization_details if provided (RFC 9396)
  if (!authorization_details.empty())
    {
      post_body += "&authorization_details=" + url_encode(hnd, authorization_details);
    }

  curl_easy_cleanup(hnd);
  return post_body;
}

void
OAuth2Authenticator::prepare_auth_credentials(CURL *hnd, std::string &post_body)
{
  switch (auth_method)
    {
    case OAUTH2_AUTH_METHOD_BASIC:
    {
      // Method 1: Use HTTP Basic Auth header
      // Credentials sent as: Authorization: Basic base64(client_id:client_secret)
      std::string userpwd = client_id + ":" + client_secret;
      curl_easy_setopt(hnd, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
      curl_easy_setopt(hnd, CURLOPT_USERPWD, userpwd.c_str());
      break;
    }
    case OAUTH2_AUTH_METHOD_POST_BODY:
    {
      // Method 2: Add credentials to POST body
      // Credentials sent as: ...&client_id=...&client_secret=...
      CURL *encode_hnd = curl_easy_init();
      if (encode_hnd)
        {
          post_body += "&client_id=" + url_encode(encode_hnd, client_id);
          post_body += "&client_secret=" + url_encode(encode_hnd, client_secret);
          curl_easy_cleanup(encode_hnd);
        }
      break;
    }
    case OAUTH2_AUTH_METHOD_CUSTOM:
    {
      // Subclasses must override this method if using CUSTOM
      msg_error("cloud_auth::oauth2::OAuth2Authenticator: "
                "CUSTOM auth method requires subclass to override prepare_auth_credentials()");
      break;
    }
    default:
    {
      g_assert_not_reached();
    }
    }
}

std::string
OAuth2Authenticator::url_encode(CURL *hnd, const std::string &value)
{
  char *encoded = curl_easy_escape(hnd, value.c_str(), value.length());
  if (!encoded)
    return "";

  std::string result(encoded);
  curl_free(encoded);
  return result;
}

size_t
OAuth2Authenticator::curl_write_callback(void *ptr, size_t size, size_t nmemb, void *userdata)
{
  std::string *response = static_cast<std::string *>(userdata);
  response->append(static_cast<char *>(ptr), size * nmemb);
  return size * nmemb;
}

bool
OAuth2Authenticator::send_token_post_request(std::string &response_payload_buffer)
{
  CURLcode ret;
  CURL *hnd = curl_easy_init();

  if (!hnd)
    {
      msg_error("cloud_auth::oauth2::OAuth2Authenticator: "
                "failed to init cURL handle",
                evt_tag_str("url", token_url.c_str()));
      return false;
    }

  std::string post_body = build_post_body();

  prepare_auth_credentials(hnd, post_body);

  curl_easy_setopt(hnd, CURLOPT_URL, token_url.c_str());
  curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "POST");
  curl_easy_setopt(hnd, CURLOPT_POSTFIELDS, post_body.c_str());
  curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, curl_write_callback);
  curl_easy_setopt(hnd, CURLOPT_WRITEDATA, (void *) &response_payload_buffer);

  ret = curl_easy_perform(hnd);
  if (ret != CURLE_OK)
    {
      msg_error("cloud_auth::oauth2::OAuth2Authenticator: "
                "error sending HTTP request to OAuth token endpoint",
                evt_tag_str("url", token_url.c_str()),
                evt_tag_str("error", curl_easy_strerror(ret)));
      curl_easy_cleanup(hnd);
      return false;
    }

  long http_result_code;
  ret = curl_easy_getinfo(hnd, CURLINFO_RESPONSE_CODE, &http_result_code);
  if (ret != CURLE_OK)
    {
      msg_error("cloud_auth::oauth2::OAuth2Authenticator: "
                "failed to get HTTP result code",
                evt_tag_str("url", token_url.c_str()),
                evt_tag_str("error", curl_easy_strerror(ret)));
      curl_easy_cleanup(hnd);
      return false;
    }

  if (http_result_code != 200)
    {
      msg_error("cloud_auth::oauth2::OAuth2Authenticator: "
                "non 200 HTTP result code received",
                evt_tag_str("url", token_url.c_str()),
                evt_tag_int("http_result_code", http_result_code),
                evt_tag_str("response", response_payload_buffer.c_str()));
      curl_easy_cleanup(hnd);
      return false;
    }

  curl_easy_cleanup(hnd);
  return true;
}

bool
OAuth2Authenticator::extract_token_from_response(const std::string &response)
{
  // Parse JSON response
  picojson::value json;
  std::string parse_error = picojson::parse(json, response);

  if (!parse_error.empty())
    {
      msg_error("cloud_auth::oauth2::OAuth2Authenticator: "
                "failed to parse JSON response from OAuth token endpoint",
                evt_tag_str("error", parse_error.c_str()),
                evt_tag_str("response", response.c_str()));
      return false;
    }

  if (!json.is<picojson::object>())
    {
      msg_error("cloud_auth::oauth2::OAuth2Authenticator: "
                "JSON response is not an object",
                evt_tag_str("response", response.c_str()));
      return false;
    }

  picojson::object obj = json.get<picojson::object>();

  // Extract access_token
  if (obj.find("access_token") == obj.end() || !obj["access_token"].is<std::string>())
    {
      msg_error("cloud_auth::oauth2::OAuth2Authenticator: "
                "access_token field not found or not a string in JSON response",
                evt_tag_str("response", response.c_str()));
      return false;
    }

  // Extract expires_in (seconds until token expiry)
  if (obj.find("expires_in") == obj.end() || !obj["expires_in"].is<double>())
    {
      msg_error("cloud_auth::oauth2::OAuth2Authenticator: "
                "expires_in field not found or not a number in JSON response",
                evt_tag_str("response", response.c_str()));
      return false;
    }

  // Store token in cache and calculate refresh time
  cached_token = obj["access_token"].get<std::string>();
  long expires_in_seconds = (long)obj["expires_in"].get<double>();

  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
  refresh_token_after = now + std::chrono::seconds{expires_in_seconds - (long)refresh_offset_seconds};

  return true;
}

/* C Wrappers */

typedef struct OAuth2Auth
{
  CloudAuthenticator super;

  gchar *client_id;
  gchar *client_secret;
  gchar *token_url;
  gchar *scope;
  gchar *resource;
  gchar *authorization_details;
  guint64 refresh_offset_seconds;
  OAuth2AuthMethod auth_method;
} _OAuth2Auth;

void
oauth2_authenticator_set_client_id(CloudAuthenticator *s, const gchar *client_id)
{
  OAuth2Auth *self = (OAuth2Auth *) s;

  g_free(self->client_id);
  self->client_id = g_strdup(client_id);
}

void
oauth2_authenticator_set_client_secret(CloudAuthenticator *s, const gchar *client_secret)
{
  OAuth2Auth *self = (OAuth2Auth *) s;

  g_free(self->client_secret);
  self->client_secret = g_strdup(client_secret);
}

void
oauth2_authenticator_set_token_url(CloudAuthenticator *s, const gchar *token_url)
{
  OAuth2Auth *self = (OAuth2Auth *) s;

  g_free(self->token_url);
  self->token_url = g_strdup(token_url);
}

void
oauth2_authenticator_set_scope(CloudAuthenticator *s, const gchar *scope)
{
  OAuth2Auth *self = (OAuth2Auth *) s;

  g_free(self->scope);
  self->scope = g_strdup(scope);
}

void
oauth2_authenticator_set_resource(CloudAuthenticator *s, const gchar *resource)
{
  OAuth2Auth *self = (OAuth2Auth *) s;

  g_free(self->resource);
  self->resource = g_strdup(resource);
}

void
oauth2_authenticator_set_authorization_details(CloudAuthenticator *s, const gchar *authorization_details)
{
  OAuth2Auth *self = (OAuth2Auth *) s;

  g_free(self->authorization_details);
  self->authorization_details = g_strdup(authorization_details);
}

void
oauth2_authenticator_set_refresh_offset(CloudAuthenticator *s, guint64 refresh_offset_seconds)
{
  OAuth2Auth *self = (OAuth2Auth *) s;

  self->refresh_offset_seconds = refresh_offset_seconds;
}

void
oauth2_authenticator_set_auth_method(CloudAuthenticator *s, OAuth2AuthMethod auth_method)
{
  OAuth2Auth *self = (OAuth2Auth *) s;

  self->auth_method = auth_method;
}

static gboolean
_init(CloudAuthenticator *s)
{
  OAuth2Auth *self = (OAuth2Auth *) s;

  try
    {
      self->super.cpp = new syslogng::cloud_auth::oauth2::OAuth2Authenticator(
        self->client_id ? self->client_id : "",
        self->client_secret ? self->client_secret : "",
        self->token_url ? self->token_url : "",
        self->scope ? self->scope : "",
        self->resource ? self->resource : "",
        self->authorization_details ? self->authorization_details : "",
        self->refresh_offset_seconds,
        self->auth_method
      );
    }
  catch (const std::runtime_error &e)
    {
      msg_error("cloud_auth::oauth2: Failed to initialize OAuth2Authenticator",
                evt_tag_str("error", e.what()));
      return FALSE;
    }

  return TRUE;
}

static void
_free(CloudAuthenticator *s)
{
  OAuth2Auth *self = (OAuth2Auth *) s;

  g_free(self->client_id);
  g_free(self->client_secret);
  g_free(self->token_url);
  g_free(self->scope);
  g_free(self->resource);
  g_free(self->authorization_details);
}

CloudAuthenticator *
oauth2_authenticator_new(void)
{
  OAuth2Auth *self = g_new0(OAuth2Auth, 1);

  self->super.init = _init;
  self->super.free_fn = _free;
  self->refresh_offset_seconds = 1800; // Default: 30 minutes
  self->auth_method = OAUTH2_AUTH_METHOD_BASIC;  // Default: use HTTP Basic Auth

  return &self->super;
}

