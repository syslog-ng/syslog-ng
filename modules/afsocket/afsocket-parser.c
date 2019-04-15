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

#include "afsocket.h"
#include "driver.h"
#include "cfg-parser.h"
#include "afsocket-grammar.h"

extern int afsocket_debug;

int afsocket_parse(CfgLexer *lexer, LogDriver **instance, gpointer arg);

static CfgLexerKeyword afsocket_keywords[] =
{
  { "unix_dgram", KW_UNIX_DGRAM },
  { "unix_stream",  KW_UNIX_STREAM },
  { "udp",                KW_UDP },
  { "tcp",                KW_TCP },
  { "syslog",             KW_SYSLOG },
  { "network",            KW_NETWORK },
#if SYSLOG_NG_ENABLE_IPV6
  { "udp6",               KW_UDP6 },
  { "tcp6",               KW_TCP6 },
#endif
  /* ssl */
  { "tls",                KW_TLS },
  { "peer_verify",        KW_PEER_VERIFY },
  { "key_file",           KW_KEY_FILE },
  { "cert_file",          KW_CERT_FILE },
  { "dhparam_file",       KW_DHPARAM_FILE },
  { "pkcs12_file",        KW_PKCS12_FILE },
  { "ca_dir",             KW_CA_DIR },
  { "crl_dir",            KW_CRL_DIR },
  { "trusted_keys",       KW_TRUSTED_KEYS },
  { "trusted_dn",         KW_TRUSTED_DN },
  { "cipher_suite",       KW_CIPHER_SUITE },
  { "ecdh_curve_list",    KW_ECDH_CURVE_LIST },
  { "curve_list",         KW_ECDH_CURVE_LIST, KWS_OBSOLETE, "ecdh_curve_list"},
  { "ssl_options",        KW_SSL_OPTIONS },
  { "allow_compress",     KW_ALLOW_COMPRESS },

  { "localip",            KW_LOCALIP },
  { "ip",                 KW_IP },
  { "interface",          KW_INTERFACE },
  { "localport",          KW_LOCALPORT },
  { "port",               KW_PORT },
  { "destport",           KW_DESTPORT },
  { "ip_ttl",             KW_IP_TTL },
  { "ip_tos",             KW_IP_TOS },
  { "ip_freebind",        KW_IP_FREEBIND },
  { "so_broadcast",       KW_SO_BROADCAST },
  { "so_rcvbuf",          KW_SO_RCVBUF },
  { "so_sndbuf",          KW_SO_SNDBUF },
  { "so_keepalive",       KW_SO_KEEPALIVE },
  { "so_reuseport",       KW_SO_REUSEPORT },
  { "tcp_keep_alive",     KW_SO_KEEPALIVE }, /* old, once deprecated form, but revived in 3.4 */
  { "tcp_keepalive",      KW_SO_KEEPALIVE }, /* alias for so-keepalive, as tcp is the only option actually using it */
  { "tcp_keepalive_time", KW_TCP_KEEPALIVE_TIME },
  { "tcp_keepalive_probes", KW_TCP_KEEPALIVE_PROBES },
  { "tcp_keepalive_intvl", KW_TCP_KEEPALIVE_INTVL },
  { "spoof_source",       KW_SPOOF_SOURCE },
  { "spoof_source_max_msglen", KW_SPOOF_SOURCE_MAX_MSGLEN },
  { "transport",          KW_TRANSPORT },
  { "ip_protocol",        KW_IP_PROTOCOL },
  { "max_connections",    KW_MAX_CONNECTIONS },
  { "listen_backlog",     KW_LISTEN_BACKLOG },
  { "keep_alive",         KW_KEEP_ALIVE },
  { "close_on_input",     KW_CLOSE_ON_INPUT },
  { "systemd_syslog",     KW_SYSTEMD_SYSLOG  },
  { "failover_servers",   KW_FAILOVER_SERVERS, KWS_OBSOLETE, "failover-servers has been deprecated, try failover() and use servers() option inside it." },
  { "failover",           KW_FAILOVER },
  { "failback",           KW_FAILBACK },
  { "servers",            KW_SERVERS },
  { "tcp_probe_interval", KW_TCP_PROBE_INTERVAL },
  { "successful_probes_required", KW_SUCCESSFUL_PROBES_REQUIRED },
  { "dynamic_window_size", KW_DYNAMIC_WINDOW_SIZE },
  { "dynamic_window_stats_freq", KW_DYNAMIC_WINDOW_STATS_FREQ },
  { "dynamic_window_realloc_ticks", KW_DYNAMIC_WINDOW_REALLOC_TICKS },
  { NULL }
};

CfgParser afsocket_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &afsocket_debug,
#endif
  .name = "afsocket",
  .keywords = afsocket_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) afsocket_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(afsocket_, LogDriver **)
