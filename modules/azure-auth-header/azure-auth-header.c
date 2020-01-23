/*
 * Copyright (c) 2020 One Identity
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

#include "azure-auth-header.h"
#include "driver.h"
#include "modules/http/http-signals.h"
#include "messages.h"
#include "compat/openssl_support.h"
#include "compat/glib.h"

#define AZURE_AUTH_HEADER_PLUGIN "azure-auth-header"

#define MAX_DATE_LEN 64

struct _AzureAuthHeaderPlugin
{
  LogDriverPlugin super;

  gsize   secret_len;
  guchar *secret;

  gchar   *workspace_id;
  gchar   *method;
  gchar   *path;
  gchar   *content_type;
};

static gsize
_get_rfc1123date(gchar *buf, gsize buf_len)
{
  time_t now = time(NULL);
  struct tm gmt;

  gmtime_r(&now, &gmt);

  gchar format[] = "%a, %d %b %Y %H:%M:%S GMT";
  gsize len = strftime(buf, buf_len, format, &gmt);

  return len;
}

static void
_get_rfc1123data_or_assert(gchar *date)
{
  gsize date_len = 0;
  date_len = _get_rfc1123date(date, MAX_DATE_LEN);
  g_assert(date_len);
}

static GString *
_azure_auth_header_get_str_to_hash(AzureAuthHeaderPlugin *self, glong content_len, const gchar *date)
{
  gchar *fmt = "%s\n%ld\n%s\nx-ms-date:%s\n%s";
  GString *str = g_string_new(NULL);

  g_string_append_printf(str, fmt, self->method, content_len, self->content_type, date, self->path);

  return str;
}

static guint
_azure_auth_header_get_digest(AzureAuthHeaderPlugin *self, GString *input, guchar *digest)
{
  guint md_len = 0;

  if (!HMAC(EVP_sha256(),
            self->secret, self->secret_len,
            (const guchar *)input->str, input->len,
            digest, &md_len))
    {
      msg_error("Failed to generate Azure Auth Header HMAC",
                evt_tag_str("str", input->str),
                evt_tag_int("len", input->len));
    }

  return md_len;
}

static void
_azure_auth_header_format_headers(AzureAuthHeaderPlugin *self, List *headers, const gchar *digest, const gchar *date)
{
  GString *auth_hdr = g_string_new(NULL);
  GString *date_hdr = g_string_new(NULL);

  g_string_printf(auth_hdr, "Authorization: SharedKey %s:%s", self->workspace_id, digest);
  g_string_printf(date_hdr, "x-ms-date: %s", date);

  list_append(headers, auth_hdr->str);
  list_append(headers, date_hdr->str);

  g_string_free(auth_hdr, TRUE);
  g_string_free(date_hdr, TRUE);
}

static gboolean
_append_headers(AzureAuthHeaderPlugin *self, List *headers, GString *body)
{
  g_return_val_if_fail(self->secret, FALSE);

  gchar date[MAX_DATE_LEN] = {0};
  _get_rfc1123data_or_assert(date);

  GString *rawstr = _azure_auth_header_get_str_to_hash(self, body->len, date);

  guchar digest[EVP_MAX_MD_SIZE] = {0};
  gsize digest_len = _azure_auth_header_get_digest(self, rawstr, digest);
  if (digest_len == 0)
    {
      g_string_free(rawstr, TRUE);
      return FALSE;
    }

  gchar *digest_str = g_base64_encode(digest, digest_len);
  _azure_auth_header_format_headers(self, headers, digest_str, date);
  g_free(digest_str);

  g_string_free(rawstr, TRUE);

  return TRUE;
}


gboolean
azure_auth_header_secret_set_from_b64str(AzureAuthHeaderPlugin *self, const gchar *b64secret)
{
  g_free(self->secret);
  self->secret = g_base64_decode(b64secret, &self->secret_len);

  return (self->secret != NULL) && (self->secret_len > 0);
}

void
azure_auth_header_workspace_id_set(AzureAuthHeaderPlugin *self, const gchar *workspace_id)
{
  g_free(self->workspace_id);
  self->workspace_id = g_strdup(workspace_id);
}

void
azure_auth_header_method_set(AzureAuthHeaderPlugin *self, const gchar *method)
{
  g_free(self->method);
  self->method = g_strdup(method);
}

void
azure_auth_header_path_set(AzureAuthHeaderPlugin *self, const gchar *path)
{
  g_free(self->path);
  self->path = g_strdup(path);
}

void
azure_auth_header_content_type_set(AzureAuthHeaderPlugin *self, const gchar *content_type)
{
  g_free(self->content_type);
  self->content_type = g_strdup(content_type);
}

static void
_slot_append_headers(AzureAuthHeaderPlugin *self, HttpHeaderRequestSignalData *data)
{
  _append_headers(self, data->request_headers, data->request_body);
}

static gboolean
_attach(LogDriverPlugin *s, LogDriver *driver)
{
  AzureAuthHeaderPlugin *self = (AzureAuthHeaderPlugin *)s;

  SignalSlotConnector *ssc = driver->super.signal_slot_connector;
  msg_debug("AzureAuthHeaderPlugin::attach()",
            evt_tag_printf("SignalSlotConnector", "%p", ssc),
            evt_tag_printf("AzureAuthHeaderPlugin", "%p", s));

  CONNECT(ssc, signal_http_header_request, _slot_append_headers, self);

  return TRUE;
}

static void
_free(LogDriverPlugin *s)
{
  AzureAuthHeaderPlugin *self = (AzureAuthHeaderPlugin *) s;

  g_free(self->workspace_id);
  g_free(self->secret);
  g_free(self->method);
  g_free(self->path);
  g_free(self->content_type);

  log_driver_plugin_free_method(s);
}

AzureAuthHeaderPlugin *
azure_auth_header_plugin_new(void)
{
  AzureAuthHeaderPlugin *self = g_new0(AzureAuthHeaderPlugin, 1);
  log_driver_plugin_init_instance(&self->super, AZURE_AUTH_HEADER_PLUGIN);

  self->super.attach = _attach;
  self->super.free_fn = _free;

  return self;
}


