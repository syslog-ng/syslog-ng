/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "afsocket.h"
#include "cfg-parser.h"
#include "afsocket-grammar.h"

extern int afsocket_debug;

int afsocket_parse(CfgLexer *lexer, LogDriver **instance, gpointer arg);

static CfgLexerKeyword afsocket_keywords[] = {
  { "unix_dgram",	        KW_UNIX_DGRAM },
  { "unix_stream",	      KW_UNIX_STREAM },
  { "udp",                KW_UDP },
  { "tcp",                KW_TCP },
  { "tcp_newline",        KW_TCP_NEWLINE },
  { "tcp_framed",         KW_TCP_FRAMED },
  { "syslog",             KW_SYSLOG },
  { "network",            KW_NETWORK },
  { "ip_protocol",        KW_IP_PROTOCOL },
#if ENABLE_IPV6
  { "udp6",               KW_UDP6 },
  { "tcp6",               KW_TCP6 },
#endif
#if ENABLE_SSL
  /* ssl */
  { "tls",                KW_TLS },
  { "peer_verify",        KW_PEER_VERIFY },
  { "key_file",           KW_KEY_FILE },
  { "cert_file",          KW_CERT_FILE },
  { "ca_dir",             KW_CA_DIR },
  { "ca_dir_layout",      KW_CA_DIR_LAYOUT},
  { "crl_dir",            KW_CRL_DIR },
  { "trusted_keys",       KW_TRUSTED_KEYS },
  { "trusted_dn",         KW_TRUSTED_DN },
  { "cipher_suite",       KW_CIPHER_SUITE },
#endif

  { "localip",            KW_LOCALIP },
  { "ip",                 KW_IP },
  { "localport",          KW_LOCALPORT },
  { "port",               KW_PORT },
  { "failover_servers",   KW_FAILOVERS },
  { "destport",           KW_DESTPORT },
  { "ip_ttl",             KW_IP_TTL },
  { "ip_tos",             KW_IP_TOS },
  { "so_broadcast",       KW_SO_BROADCAST },
  { "so_rcvbuf",          KW_SO_RCVBUF },
  { "so_sndbuf",          KW_SO_SNDBUF },
  { "so_keepalive",       KW_SO_KEEPALIVE },
  { "tcp_keep_alive",     KW_SO_KEEPALIVE, 0, KWS_OBSOLETE, "so_keepalive" },
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
