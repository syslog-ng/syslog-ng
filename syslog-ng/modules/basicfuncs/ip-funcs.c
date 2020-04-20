/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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

#include "gsocket.h"
#include "host-resolve.h"
#include "cfg-parser.h"

static void
tf_ipv4_to_int(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  gint i;

  for (i = 0; i < argc; i++)
    {
      struct in_addr ina;

      g_inet_aton(argv[i]->str, &ina);
      g_string_append_printf(result, "%lu", (gulong) ntohl(ina.s_addr));
      if (i < argc - 1)
        g_string_append_c(result, ',');
    }
}

TEMPLATE_FUNCTION_SIMPLE(tf_ipv4_to_int);

typedef struct _DnsResolveIpState
{
  TFSimpleFuncState super;
  HostResolveOptions host_resolve_options;
} DnsResolveIpState;

static gboolean
_process_use_fqdn(const gchar *option_name, const gchar *value, gpointer data, GError **error)
{
  HostResolveOptions *host_resolve_options = (HostResolveOptions *)data;
  host_resolve_options->use_fqdn = cfg_process_yesno(value);
  return TRUE;
}

static gboolean
_process_use_dns(const gchar *option_name, const gchar *value, gpointer data, GError **error)
{
  HostResolveOptions *host_resolve_options = (HostResolveOptions *)data;
  host_resolve_options->use_dns = cfg_process_yesno(value);
  return TRUE;
}

static gboolean
_process_dns_cache(const gchar *option_name, const gchar *value, gpointer data, GError **error)
{
  HostResolveOptions *host_resolve_options = (HostResolveOptions *)data;
  host_resolve_options->use_dns_cache = cfg_process_yesno(value);
  return TRUE;
}

static gboolean
_process_normalize_hostnames(const gchar *option_name, const gchar *value, gpointer data, GError **error)
{
  HostResolveOptions *host_resolve_options = (HostResolveOptions *)data;
  host_resolve_options->normalize_hostnames = cfg_process_yesno(value);
  return TRUE;
}

static gboolean
_parse_host_resolve_options(gint *argc, gchar **argv[], HostResolveOptions *host_resolve_options, GError **error)
{
  gboolean result = FALSE;
  GOptionEntry entries[] =
  {
    { "use-fqdn", 'f', 0, G_OPTION_ARG_CALLBACK, _process_use_fqdn, NULL, NULL },
    { "use-dns", 'd', 0, G_OPTION_ARG_CALLBACK, _process_use_dns, NULL, NULL },
    { "dns-cache", 'c', 0, G_OPTION_ARG_CALLBACK, _process_dns_cache, NULL, NULL },
    { "normalize-hostnames", 'n', 0, G_OPTION_ARG_CALLBACK, _process_normalize_hostnames, NULL, NULL },
    { NULL }
  };

  GOptionContext *ctx = g_option_context_new(*argv[0]);
  GOptionGroup *group = g_option_group_new("host-resolve-options", NULL, NULL, host_resolve_options, NULL);
  g_option_group_add_entries(group, entries);
  g_option_context_set_main_group(ctx, group);

  if (!g_option_context_parse(ctx, argc, argv, error))
    goto exit;

  result = TRUE;
exit:
  g_option_context_free(ctx);
  return result;
}

static gboolean
_setup_host_resolve_options(DnsResolveIpState *state, LogTemplate *parent, gint *argc, gchar **argv[], GError **error)
{
  host_resolve_options_defaults(&state->host_resolve_options);

  if (!_parse_host_resolve_options(argc, argv, &state->host_resolve_options, error))
    return FALSE;

  host_resolve_options_init(&state->host_resolve_options, &parent->cfg->host_resolve_options);

  return TRUE;
}

static gboolean
tf_dns_resolve_ip_prepare(LogTemplateFunction *self, gpointer s, LogTemplate *parent, gint argc, gchar *argv[],
                          GError **error)
{
  DnsResolveIpState *state = (DnsResolveIpState *)s;

  if (!_setup_host_resolve_options(state, parent, &argc, &argv, error))
    return FALSE;

  if (argc > 2)
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, 0,
                  "dns-resolve-ip: too many arguments provided. usage: $(dns-resolve-ip [OPTIONS] IP)");
      return FALSE;
    }

  if (!tf_simple_func_prepare(self, s, parent, argc, argv, error))
    return FALSE;

  return TRUE;
}

static void
tf_dns_resolve_ip_call(LogTemplateFunction *self, gpointer s, const LogTemplateInvokeArgs *args, GString *result)
{
  DnsResolveIpState *state = (DnsResolveIpState *)s;
  const gchar *hostname;
  gsize result_len;

  gchar *ip = args->argv[0]->str;
  GSockAddr *addr = g_sockaddr_inet_or_inet6_new(ip, 0);
  if (!addr)
    return;

  hostname = resolve_sockaddr_to_hostname(&result_len, addr, &state->host_resolve_options);
  g_string_append_len(result, hostname, result_len);

  g_sockaddr_unref(addr);
}

TEMPLATE_FUNCTION(DnsResolveIpState, tf_dns_resolve_ip, tf_dns_resolve_ip_prepare, tf_simple_func_eval,
                  tf_dns_resolve_ip_call, tf_simple_func_free_state, NULL);
