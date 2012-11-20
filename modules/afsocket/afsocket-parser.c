/*
 * Copyright (c) 2002-2012 BalaBit IT Ltd, Budapest, Hungary
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

static CfgLexerKeyword afsocket_keywords[] = {
  { "unix_dgram",	KW_UNIX_DGRAM },
  { "unix_stream",	KW_UNIX_STREAM },
  { "udp",                KW_UDP },
  { "tcp",                KW_TCP },
  { "syslog",             KW_SYSLOG },
  { "network",            KW_NETWORK, 0x0304 },
#if ENABLE_IPV6
  { "udp6",               KW_UDP6 },
  { "tcp6",               KW_TCP6 },
#endif
#if BUILD_WITH_SSL
  /* ssl */
  { "tls",                KW_TLS },
  { "peer_verify",        KW_PEER_VERIFY },
  { "key_file",           KW_KEY_FILE },
  { "cert_file",          KW_CERT_FILE },
  { "ca_dir",             KW_CA_DIR },
  { "crl_dir",            KW_CRL_DIR },
  { "trusted_keys",       KW_TRUSTED_KEYS },
  { "trusted_dn",         KW_TRUSTED_DN },
  { "cipher_suite",       KW_CIPHER_SUITE },
#endif

  { "localip",            KW_LOCALIP },
  { "ip",                 KW_IP },
  { "localport",          KW_LOCALPORT },
  { "port",               KW_PORT },
  { "destport",           KW_DESTPORT },
  { "ip_ttl",             KW_IP_TTL },
  { "ip_tos",             KW_IP_TOS },
  { "so_broadcast",       KW_SO_BROADCAST },
  { "so_rcvbuf",          KW_SO_RCVBUF },
  { "so_sndbuf",          KW_SO_SNDBUF },
  { "so_keepalive",       KW_SO_KEEPALIVE },
  { "tcp_keep_alive",     KW_SO_KEEPALIVE }, /* old, once deprecated form, but revived in 3.4 */
  { "tcp_keepalive",      KW_SO_KEEPALIVE, 0x0304 }, /* alias for so-keepalive, as tcp is the only option actually using it */
  { "tcp_keepalive_time", KW_TCP_KEEPALIVE_TIME, 0x0304 },
  { "tcp_keepalive_probes", KW_TCP_KEEPALIVE_PROBES, 0x0304 },
  { "tcp_keepalive_intvl", KW_TCP_KEEPALIVE_INTVL, 0x0304 },
  { "spoof_source",       KW_SPOOF_SOURCE },
  { "transport",          KW_TRANSPORT },
  { "max_connections",    KW_MAX_CONNECTIONS },
  { "keep_alive",         KW_KEEP_ALIVE },
  { NULL }
};

CfgParser afsocket_parser =
{
#if ENABLE_DEBUG
  .debug_flag = &afsocket_debug,
#endif
  .name = "afsocket",
  .keywords = afsocket_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) afsocket_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(afsocket_, LogDriver **)
