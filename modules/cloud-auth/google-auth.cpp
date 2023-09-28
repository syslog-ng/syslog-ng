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

#include <exception>
#include <fstream>

#include "compat/cpp-start.h"
#include "scratch-buffers.h"
#include "compat/cpp-end.h"

using namespace syslogng::cloud_auth::google;

ServiceAccountAuthenticator::ServiceAccountAuthenticator(const char *key_path, const char *audience_)
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
  std::chrono::system_clock::time_point expires_at = issued_at + std::chrono::seconds{3600};

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

/* C Wrappers */

typedef struct _GoogleAuthenticator
{
  CloudAuthenticator super;
  GoogleAuthenticatorAuthMode auth_mode;

  struct
  {
    gchar *key_path;
    gchar *audience;
  } service_account_options;
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
                                                            self->service_account_options.audience);
        }
      catch (const std::runtime_error &e)
        {
          msg_error("cloud_auth::google: Failed to initialize ServiceAccountAuthenticator",
                    evt_tag_str("error", e.what()));
          return FALSE;
        }
      break;
    case GAAM_UNDEFINED:
    default:
      g_assert_not_reached();
    }

  return TRUE;
}

static void
_free(CloudAuthenticator *s)
{
  GoogleAuthenticator *self = (GoogleAuthenticator *) s;

  g_free(self->service_account_options.key_path);
  g_free(self->service_account_options.audience);
}

CloudAuthenticator *
google_authenticator_new(void)
{
  GoogleAuthenticator *self = g_new0(GoogleAuthenticator, 1);

  self->super.init = _init;
  self->super.free_fn = _free;

  return &self->super;
}
