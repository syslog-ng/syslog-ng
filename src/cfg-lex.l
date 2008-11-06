/***************************************************************************
 *
 * Copyright (c) 1999 Balázs Scheidler
 * Copyright (c) 1999-2007 BalaBit IT Ltd.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Inspired by nsyslog, originally written by Darren Reed.
 *
 * $Id: cfg-lex.l,v 1.8 2003/01/22 11:11:18 bazsi Exp $
 *
 ***************************************************************************/
%{

#include "syslog-ng.h"
#include "filter.h"
#include "cfg-grammar.h"
#include "messages.h"

#include <string.h>
#include <strings.h>

struct keyword 
{
  char	*kw_name;
  int	kw_token;
  int   kw_status;
  char  *kw_explain;
};

#define KWS_NORMAL        0
#define KWS_OBSOLETE      1

static struct keyword keywords[] = {
	/* statements */
	{ "source", 		KW_SOURCE },
	{ "filter",             KW_FILTER },
	{ "parser",             KW_PARSER },
	{ "rewrite",            KW_REWRITE },
	{ "destination",	KW_DESTINATION },
	{ "log",		KW_LOG },
	{ "options",		KW_OPTIONS },

	/* source or destination items */
	{ "file",		KW_FILE },
        { "fifo",               KW_PIPE },
	{ "pipe",		KW_PIPE },
        { "internal",           KW_INTERNAL },
	{ "unix_dgram",		KW_UNIX_DGRAM },
	{ "unix_stream",	KW_UNIX_STREAM },
        { "udp",                KW_UDP },
        { "tcp",                KW_TCP },
#if ENABLE_IPV6
        { "udp6",               KW_UDP6 },
        { "tcp6",               KW_TCP6 },
#endif
        { "syslog",             KW_SYSLOG },
        { "usertty", 		KW_USERTTY },
        { "door",               KW_DOOR },
#if ENABLE_SUN_STREAMS
        { "sun_stream",		KW_SUN_STREAMS },
        { "sun_streams",	KW_SUN_STREAMS },
#endif
        { "program",		KW_PROGRAM },
/* BEGIN MARK: sql */
#if ENABLE_SQL
        { "sql",                KW_SQL },
        { "username",           KW_USERNAME },
        { "password",           KW_PASSWORD },
        { "database",           KW_DATABASE },
        { "encoding",           KW_ENCODING },
        { "table",              KW_TABLE },
        
        { "columns",            KW_COLUMNS },
        { "indexes",            KW_INDEXES },
        { "values",             KW_VALUES },
#endif
/* END MARK */
        { "columns",            KW_COLUMNS },
        { "delimiters",         KW_DELIMITERS },
        { "quotes",             KW_QUOTES },
        { "quote_pairs",        KW_QUOTE_PAIRS },
        { "null",               KW_NULL },
        { "csv_parser",         KW_CSV_PARSER },
        { "db_parser",          KW_DB_PARSER },

	/* option items */
	{ "flags",		KW_FLAGS },
	{ "pad_size",		KW_PAD_SIZE },
	{ "mark_freq",		KW_MARK_FREQ },
	{ "mark",		KW_MARK_FREQ, KWS_OBSOLETE, "mark_freq" },
	{ "stats_freq",		KW_STATS_FREQ },
	{ "stats_level",	KW_STATS_LEVEL },
	{ "stats",		KW_STATS_FREQ, KWS_OBSOLETE, "stats_freq" },
	{ "flush_lines", 	KW_FLUSH_LINES },
	{ "flush_timeout", 	KW_FLUSH_TIMEOUT },
        { "suppress",           KW_SUPPRESS },
	{ "sync_freq", 		KW_FLUSH_LINES, KWS_OBSOLETE, "flush_lines" },
	{ "sync", 		KW_FLUSH_LINES, KWS_OBSOLETE, "flush_lines" },
	{ "fsync",		KW_FSYNC },
	{ "long_hostnames",	KW_CHAIN_HOSTNAMES },
        { "chain_hostnames",    KW_CHAIN_HOSTNAMES },
        { "normalize_hostnames",KW_NORMALIZE_HOSTNAMES },
        { "keep_hostname",      KW_KEEP_HOSTNAME },
        { "check_hostname",     KW_CHECK_HOSTNAME },
        { "bad_hostname",       KW_BAD_HOSTNAME },
        { "keep_timestamp",	KW_KEEP_TIMESTAMP },
        { "encoding", 		KW_ENCODING },
        { "ts_format",		KW_TS_FORMAT },
        { "frac_digits",        KW_FRAC_DIGITS },
        { "time_zone",		KW_TIME_ZONE },
        { "recv_time_zone",	KW_RECV_TIME_ZONE },
        { "send_time_zone",	KW_SEND_TIME_ZONE },
        { "use_time_recvd",	KW_USE_TIME_RECVD, KWS_OBSOLETE, "Use R_ or S_ prefixed macros in templates" },
        { "use_fqdn",           KW_USE_FQDN },
	{ "use_dns",		KW_USE_DNS },
  	{ "gc_threshold",	KW_GC_BUSY_THRESHOLD },
  	{ "gc_busy_threshold",	KW_GC_BUSY_THRESHOLD },
  	{ "gc_idle_threshold",	KW_GC_IDLE_THRESHOLD },
 	{ "time_reopen",	KW_TIME_REOPEN },
 	{ "time_reap",          KW_TIME_REAP },
 	{ "time_sleep",         KW_TIME_SLEEP },
 	{ "follow_freq",	KW_FOLLOW_FREQ,  },
 	{ "remove_if_older",	KW_OVERWRITE_IF_OLDER, KWS_OBSOLETE, "overwrite_if_older" },
 	{ "overwrite_if_older",	KW_OVERWRITE_IF_OLDER },
 	{ "file_template",	KW_FILE_TEMPLATE },
 	{ "proto_template",	KW_PROTO_TEMPLATE },
#if ENABLE_TIMESTAMPING
 	{ "timestamp_url",      KW_TIMESTAMP_URL },
	{ "timestamp_freq",     KW_TIMESTAMP_FREQ },
#endif
        { "set",                KW_SET },
        { "subst",              KW_SUBST },
        { "value",              KW_VALUE },
 	
	{ "log_fifo_size",	KW_LOG_FIFO_SIZE },
	{ "log_disk_fifo_size",	KW_LOG_DISK_FIFO_SIZE },
	{ "log_fetch_limit",	KW_LOG_FETCH_LIMIT },
	{ "log_iw_size",	KW_LOG_IW_SIZE },
	{ "log_msg_size",	KW_LOG_MSG_SIZE },
	{ "log_prefix",		KW_LOG_PREFIX, KWS_OBSOLETE, "program_override" },
	{ "program_override",   KW_PROGRAM_OVERRIDE },
	{ "host_override",      KW_HOST_OVERRIDE },
	{ "throttle",           KW_THROTTLE },
	
        { "create_dirs",        KW_CREATE_DIRS },
        { "optional",           KW_OPTIONAL },
 	{ "localip",		KW_LOCALIP },
	{ "ip",			KW_IP },
	{ "localport",		KW_LOCALPORT },
	{ "port",		KW_PORT },
	{ "destport",		KW_DESTPORT },
        { "framed",             KW_FRAMED },
        { "ip_ttl",             KW_IP_TTL },
        { "ip_tos",             KW_IP_TOS },
        { "so_broadcast",       KW_SO_BROADCAST },
        { "so_rcvbuf",          KW_SO_RCVBUF },
        { "so_sndbuf",          KW_SO_SNDBUF },
        { "so_keepalive",       KW_SO_KEEPALIVE },
        { "tcp_keep_alive",     KW_SO_KEEPALIVE, KWS_OBSOLETE, "so_keepalive" },
        { "spoof_source",       KW_SPOOF_SOURCE },
        { "transport",          KW_TRANSPORT },

	{ "owner",		KW_OWNER },
	{ "group",		KW_GROUP },
	{ "perm",		KW_PERM },
	{ "dir_owner",		KW_DIR_OWNER },
	{ "dir_group",		KW_DIR_GROUP },
        { "dir_perm",           KW_DIR_PERM },
        { "template",           KW_TEMPLATE },
        { "template_escape",	KW_TEMPLATE_ESCAPE },
 	{ "keep_alive",         KW_KEEP_ALIVE },
	{ "max_connections",	KW_MAX_CONNECTIONS },
	{ "mac",		KW_MAC },
	{ "authentication",	KW_AUTH },
	{ "encrypt",		KW_ENCRYPT },
	{ "compress",		KW_COMPRESS },
	{ "persist_only",       KW_PERSIST_ONLY },
	{ "dns_cache_hosts",    KW_DNS_CACHE_HOSTS },
	{ "dns_cache",		KW_DNS_CACHE },
	{ "dns_cache_size",	KW_DNS_CACHE_SIZE },
	{ "dns_cache_expire",	KW_DNS_CACHE_EXPIRE },
	{ "dns_cache_expire_failed", KW_DNS_CACHE_EXPIRE_FAILED },

#if ENABLE_SSL /* BEGIN MARK: tls */
	/* ssl */
	{ "tls",		KW_TLS },
	{ "peer_verify",        KW_PEER_VERIFY },
	{ "key_file",		KW_KEY_FILE },
	{ "cert_file",		KW_CERT_FILE },
	{ "ca_dir",		KW_CA_DIR },
	{ "crl_dir",		KW_CRL_DIR },
        { "trusted_keys",       KW_TRUSTED_KEYS },
        { "trusted_dn",         KW_TRUSTED_DN },
#endif /* END MARK */

	/* filter items */
        { "or",                 KW_OR },
	{ "and",                KW_AND },
        { "not",                KW_NOT },
	{ "level",              KW_LEVEL },
	{ "priority",           KW_LEVEL },
	{ "facility",           KW_FACILITY },
	{ "program",		KW_PROGRAM },
        { "host",               KW_HOST },
        { "message",            KW_MESSAGE },
        { "match",		KW_MATCH },
        { "netmask",		KW_NETMASK },
        { "type",               KW_TYPE },

	/* on/off switches */
	{ "yes",		KW_YES },
	{ "on",			KW_YES },
	{ "no",			KW_NO },
	{ "off", 		KW_NO }
};

#define MAX_REGEXP_LEN	1024

int linenum = 1;
int lex_filter_params = 0;
char buf[MAX_REGEXP_LEN];
char *str;

static int check_reserved_words(char *token);
static void append_string(int length, char *str);
static void append_char(char c);

%}

%option nounput
%option noyywrap

white	[ \t]
digit	[0-9]
alpha		[a-zA-Z]
alphanum	[a-zA-Z0-9]
word	[^ \#'"\(\)\{\}\\;\n\t,|\.]

%x string
%x qstring
%%

\#.*$                      ;
\r?\n			   { linenum++; }
{white}+		   ;
\.\.                       { return DOTDOT; }
0x{digit}+ 		   { yylval.num = strtoll(yytext, NULL, 16); return NUMBER; }
0{digit}+		   { yylval.num = strtoll(yytext, NULL, 8); return NUMBER; }
(-|\+)?{digit}+            { yylval.num = strtoll(yytext, NULL, 10); return NUMBER; }
({word}+(\.)?)*{word}+ 	   { return check_reserved_words(yytext); }
\(	      		   { return '('; }
\)			   { return ')'; }
\;			   { return ';'; }
\{			   { return '{'; }
\}			   { return '}'; }
\|			   { return '|'; }
\,			   ;

\"                         {
				str = buf;
				/* yy_push_state(string);*/
				BEGIN(string);
			   }
\'			   {
				str = buf;
				BEGIN(qstring);
			   }
<string>\\a		   { append_char(7); }
<string>\\n	   	   { append_char(10); }
<string>\\r		   { append_char(13); }
<string>\\t		   { append_char(9); }
<string>\\v		   { append_char(11); }
<string>\\[^anrtv]	   { append_string(1, yytext + 1); }
<string>\"		   { 
				BEGIN(INITIAL);
				/* yy_pop_state();*/
				yylval.cptr = strdup(buf);
				return STRING; 
		           }
<string>[^"\\]+		   { append_string(strlen(yytext), yytext); }
<qstring>[^']+		   { append_string(strlen(yytext), yytext); }
<qstring>\'		   { 
				BEGIN(INITIAL);
				yylval.cptr = strdup(buf);
				return STRING;
			   }

%%
int 
lex_init(FILE *file, gint init_line_num)
{
  yyrestart(file);
  linenum = init_line_num;
  return 0;
}

int 
check_reserved_words(char *token)
{
  int i, j;
	
  for (i = 0; i < (sizeof(keywords) / sizeof(struct keyword)); i++) 
    {
      for (j = 0; token[j] && keywords[i].kw_name[j]; j++)
        {
          if (token[j] == '-' || token[j] == '_')
            {
              if (keywords[i].kw_name[j] != '_')
                break;
            }
          else if (token[j] != keywords[i].kw_name[j])
            break;
        }
      if (token[j] == 0 && keywords[i].kw_name[j] == 0)
        {
        
          switch (keywords[i].kw_status)
            {
            case KWS_OBSOLETE:
              msg_warning("Your configuration file uses an obsoleted keyword, please update your configuration",
                          evt_tag_str("keyword", keywords[i].kw_name),
                          evt_tag_str("change", keywords[i].kw_explain),
                          NULL);
              break;
            default:
              break;
            }
          keywords[i].kw_status = KWS_NORMAL;
          return keywords[i].kw_token;
        }
    }
  yylval.cptr = strdup(token);
  return IDENTIFIER;
}


static void 
append_string(int length, char *s)
{
  int to_copy = MIN(MAX_REGEXP_LEN - (str - buf) - 1, length);

  memcpy(str, s, to_copy);
  str += to_copy;
  *str = 0;
}

static void 
append_char(char c)
{
  *str = c;
  str++;
  *str = 0;
}
