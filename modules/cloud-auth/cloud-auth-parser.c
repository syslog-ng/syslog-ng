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

#include "cloud-auth.h"
#include "cfg-parser.h"
#include "cloud-auth-grammar.h"

extern int cloud_auth_debug;
int cloud_auth_parse(CfgLexer *lexer, LogDriverPlugin **instance, gpointer arg);

static CfgLexerKeyword cloud_auth_keywords[] =
{
  { "cloud_auth",               KW_CLOUD_AUTH },

  { "gcp",                          KW_GCP },
  { "service_account",              KW_SERVICE_ACCOUNT },
  {   "audience",                   KW_AUDIENCE },
  {   "key",                        KW_KEY },
  {   "scope",                      KW_SCOPE },
  {   "token_validity_duration",    KW_TOKEN_VALIDITY_DURATION },
  { "user_managed_service_account", KW_USER_MANAGED_SERVICE_ACCOUNT },
  {   "metadata_url",               KW_METADATA_URL },
  {   "name",                       KW_NAME },

  { "azure",                         KW_AZURE },
  { "app_id",                        KW_APP_ID },
  { "app_secret",                    KW_APP_SECRET },
  { "monitor",                       KW_MONITOR },
  { "tenant_id",                     KW_TENANT_ID },

  { "oauth2",                        KW_OAUTH2 },
  { "auth_method",                   KW_AUTH_METHOD },
  {   "basic",                       KW_BASIC },
  {   "post_body",                   KW_POST_BODY },
  { "authorization_details",         KW_AUTHORIZATION_DETAILS },
  { "client_id",                     KW_CLIENT_ID },
  { "client_secret",                 KW_CLIENT_SECRET },
  { "refresh_offset",                KW_REFRESH_OFFSET },
  { "resource",                      KW_RESOURCE },
  // scope is reused from GCP
  { "token_url",                     KW_TOKEN_URL },

  { NULL }
};

CfgParser cloud_auth_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &cloud_auth_debug,
#endif
  .name = "cloud_auth",
  .keywords = cloud_auth_keywords,
  .parse = (int (*)(CfgLexer * lexer, gpointer * instance, gpointer arg)) cloud_auth_parse,
  .cleanup = (void (*)(gpointer)) log_driver_plugin_free,

};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(cloud_auth_, CLOUD_AUTH_, LogDriverPlugin **)
