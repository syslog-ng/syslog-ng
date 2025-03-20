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
#include "azure-auth.hpp"

#include "compat/cpp-start.h"
#include "scratch-buffers.h"
#include "compat/cpp-end.h"

#include <fstream>

using namespace syslogng::cloud_auth::azure;

AzureMonitorAuthenticator::AzureMonitorAuthenticator(const char *tenant_id,
                                                     const char *app_id,
                                                     const char *app_secret,
                                                     const char *scope)
{
  auth_url = "https://login.microsoftonline.com/";
  auth_url.append(tenant_id);
  auth_url.append("/oauth2/v2.0/token");

  auth_body = "grant_type=client_credentials&client_id=";
  auth_body.append(app_id);
  auth_body.append("&client_secret=");
  auth_body.append(app_secret);
  auth_body.append("&scope=");
  auth_body.append(scope);
}

void AzureMonitorAuthenticator::handle_http_header_request(HttpHeaderRequestSignalData *data)
{
  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

  lock.lock();

  if (now <= refresh_token_after && !cached_token.empty())
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

  long expiry;
  if (!parse_token_and_expiry_from_response(response_payload_buffer, cached_token, &expiry))
    {
      lock.unlock();

      data->result = HTTP_SLOT_CRITICAL_ERROR;
      return;
    }

  refresh_token_after = now + std::chrono::seconds{expiry - 60};
  add_token_to_header(data);

  lock.unlock();

  data->result = HTTP_SLOT_SUCCESS;
}

void
AzureMonitorAuthenticator::add_token_to_header(HttpHeaderRequestSignalData *data)
{
  /* Scratch Buffers are marked at this point in http-worker.c */
  GString *auth_buffer = scratch_buffers_alloc();
  g_string_append(auth_buffer, "Authorization: Bearer ");
  g_string_append(auth_buffer, cached_token.c_str());

  list_append(data->request_headers, auth_buffer->str);
}

bool
AzureMonitorAuthenticator::send_token_post_request(std::string &response_payload_buffer)
{
  CURLcode ret;
  CURL *hnd = curl_easy_init();

  if (!hnd)
    {
      msg_error("cloud_auth::azure::AzureMonitorAuthenticator: "
                "failed to init cURL handle",
                evt_tag_str("url", auth_url.c_str()));
      goto error;
    }

  curl_easy_setopt(hnd, CURLOPT_URL, auth_url.c_str());
  curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "POST");
  curl_easy_setopt(hnd, CURLOPT_POSTFIELDS, auth_body.c_str());
  curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, curl_write_callback);
  curl_easy_setopt(hnd, CURLOPT_WRITEDATA, (void *) &response_payload_buffer);

  ret = curl_easy_perform(hnd);
  if (ret != CURLE_OK)
    {
      msg_error("cloud_auth::google::UserManagedServiceAccountAuthenticator: "
                "error sending HTTP request to metadata server",
                evt_tag_str("url", auth_url.c_str()),
                evt_tag_str("error", curl_easy_strerror(ret)));
      goto error;
    }

  long http_result_code;
  ret = curl_easy_getinfo(hnd, CURLINFO_RESPONSE_CODE, &http_result_code);
  if (ret != CURLE_OK)
    {
      msg_error("cloud_auth::google::UserManagedServiceAccountAuthenticator: "
                "failed to get HTTP result code",
                evt_tag_str("url", auth_url.c_str()),
                evt_tag_str("error", curl_easy_strerror(ret)));
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

  curl_easy_cleanup(hnd);
  return true;

error:
  if (hnd)
    {
      curl_easy_cleanup(hnd);
    }
  return false;
}

bool
AzureMonitorAuthenticator::parse_token_and_expiry_from_response(const std::string &response_payload,
    std::string &token, long *expiry)
{
  picojson::value json;
  std::string json_parse_error = picojson::parse(json, response_payload);
  if (!json_parse_error.empty())
    {
      msg_error("cloud_auth::azure::AzureMonitorAuthenticator: "
                "failed to parse response JSON",
                evt_tag_str("url", auth_url.c_str()),
                evt_tag_str("response_json", response_payload.c_str()));
      return false;
    }

  if (!json.is<picojson::object>() || !json.contains("access_token") || !json.contains("expires_in"))
    {
      msg_error("cloud_auth::azure::AzureMonitorAuthenticator: "
                "unexpected response JSON",
                evt_tag_str("url", auth_url.c_str()),
                evt_tag_str("response_json", response_payload.c_str()));
      return false;
    }

  token.assign(json.get("access_token").get<std::string>());
  *expiry = lround(json.get("expires_in").get<double>()); /* getting a long from picojson is not always available */
  return true;
}

size_t
AzureMonitorAuthenticator::curl_write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
  const char *data = (const char *) contents;
  std::string *response_payload_buffer = (std::string *) userp;

  size_t real_size = size * nmemb;
  response_payload_buffer->append(data, real_size);

  return real_size;
}

/* C Wrappers */

typedef struct AzureAuthenticator
{
  CloudAuthenticator super;

  AzureAuthenticatorAuthMode auth_mode;

  gchar *tenant_id;
  gchar *app_id;
  gchar *scope;
  gchar *app_secret;
} _AzureAuthenticator;

void
azure_authenticator_set_auth_mode(CloudAuthenticator *s, AzureAuthenticatorAuthMode auth_mode)
{
  AzureAuthenticator *self = (AzureAuthenticator *) s;

  self->auth_mode = auth_mode;
}

void
azure_authenticator_set_tenant_id(CloudAuthenticator *s, const gchar *tenant_id)
{
  AzureAuthenticator *self = (AzureAuthenticator *) s;

  g_free(self->tenant_id);
  self->tenant_id = g_strdup(tenant_id);
}

void
azure_authenticator_set_app_id(CloudAuthenticator *s, const gchar *app_id)
{
  AzureAuthenticator *self = (AzureAuthenticator *) s;

  g_free(self->app_id);
  self->app_id = g_strdup(app_id);
}

void
azure_authenticator_set_scope(CloudAuthenticator *s, const gchar *scope)
{
  AzureAuthenticator *self = (AzureAuthenticator *) s;

  g_free(self->scope);
  self->scope = g_strdup(scope);
}

void
azure_authenticator_set_app_secret(CloudAuthenticator *s, const gchar *app_secret)
{
  AzureAuthenticator *self = (AzureAuthenticator *) s;

  g_free(self->app_secret);
  self->app_secret = g_strdup(app_secret);
}

static gboolean
_init(CloudAuthenticator *s)
{
  AzureAuthenticator *self = (AzureAuthenticator *) s;

  switch (self->auth_mode)
    {
    case AAAM_MONITOR:
      try
        {
          self->super.cpp = new AzureMonitorAuthenticator(self->tenant_id,
                                                          self->app_id,
                                                          self->app_secret,
                                                          self->scope);
        }
      catch (const std::runtime_error &e)
        {
          msg_error("cloud_auth::azure: Failed to initialize AzureMonitorAuthenticator",
                    evt_tag_str("error", e.what()));
          return FALSE;
        }
      break;
    case AAAM_UNDEFINED:
      msg_error("cloud_auth::azure: Failed to initialize AzureMonitorAuthenticator",
                evt_tag_str("error", "Authentication mode must be set (e.g. monitor())"));
      return FALSE;
    default:
      g_assert_not_reached();
    }

  return TRUE;
}

static void
_free(CloudAuthenticator *s)
{
  AzureAuthenticator *self = (AzureAuthenticator *) s;

  g_free(self->tenant_id);
  g_free(self->app_id);
  g_free(self->scope);
  g_free(self->app_secret);
}

static void
_set_default_options(AzureAuthenticator *self)
{
  self->scope = g_strdup("https://monitor.azure.com//.default");
}

CloudAuthenticator *
azure_authenticator_new(void)
{
  AzureAuthenticator *self = g_new0(AzureAuthenticator, 1);

  self->super.init = _init;
  self->super.free_fn = _free;

  _set_default_options(self);

  return &self->super;
}
