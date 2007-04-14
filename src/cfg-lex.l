/***************************************************************************
 *
 * Copyright (c) 1999 Balázs Scheidler
 * Copyright (c) 1999-2001 BalaBit IT Ltd.
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
#include "logreader.h"
#include "messages.h"

#include <string.h>
#include <strings.h>

struct keyword {
	char	*kw_name;
	int	kw_token;
};

static struct keyword keywords[] = {
	/* statements */
	{ "source", 		KW_SOURCE },
	{ "destination",	KW_DESTINATION },
	{ "log",		KW_LOG },
	{ "options",		KW_OPTIONS },
	{ "filter",             KW_FILTER },

	/* source or destination items */
	{ "file",		KW_FILE },
        { "fifo",               KW_PIPE },
	{ "pipe",		KW_PIPE },
        { "internal",           KW_INTERNAL },
	{ "unix_dgram",		KW_UNIX_DGRAM },
	{ "unix_stream",	KW_UNIX_STREAM },
        { "udp",                KW_UDP },
        { "tcp",                KW_TCP },
        { "udp6",               KW_UDP6 },
        { "tcp6",               KW_TCP6 },
        { "usertty", 		KW_USER },
        { "door",               KW_DOOR },
        { "sun_stream",		KW_SUN_STREAMS },
        { "sun_streams",	KW_SUN_STREAMS },
        { "program",		KW_PROGRAM },

	/* option items */
	{ "flags",		KW_FLAGS },
	{ "catchall",		KW_CATCHALL },
	{ "fallback",		KW_FALLBACK },
	{ "final",		KW_FINAL },
	{ "flow_control",	KW_FLOW_CONTROL },
	{ "pad_size",		KW_PAD_SIZE },
	{ "mark_freq",		KW_MARK_FREQ },
	{ "stats_freq",		KW_STATS_FREQ },
	{ "stats",		KW_STATS_FREQ },   /* obsolete */
	{ "flush_lines", 	KW_FLUSH_LINES },
	{ "flush_timeout", 	KW_FLUSH_TIMEOUT },
	/* obsolete */
	{ "sync_freq", 		KW_FLUSH_LINES },  /* obsolete */
	{ "sync", 		KW_FLUSH_LINES },  /* obsolete */
	{ "fsync",		KW_FSYNC },
	{ "long_hostnames",	KW_CHAIN_HOSTNAMES },
        { "chain_hostnames",    KW_CHAIN_HOSTNAMES },
        { "normalize_hostnames",KW_NORMALIZE_HOSTNAMES },
        { "keep_hostname",      KW_KEEP_HOSTNAME },
        { "check_hostname",     KW_CHECK_HOSTNAME },
        { "bad_hostname",       KW_BAD_HOSTNAME },
        { "keep_timestamp",	KW_KEEP_TIMESTAMP },
        { "ts_format",		KW_TS_FORMAT },
        { "frac_digits",        KW_FRAC_DIGITS },
        { "time_zone",		KW_TIME_ZONE },
        { "recv_time_zone",	KW_RECV_TIME_ZONE },
        { "send_time_zone",	KW_SEND_TIME_ZONE },
        { "use_time_recvd",	KW_USE_TIME_RECVD },
        { "use_fqdn",           KW_USE_FQDN },
	{ "use_dns",		KW_USE_DNS },
  	{ "gc_threshold",	KW_GC_BUSY_THRESHOLD },
  	{ "gc_busy_threshold",	KW_GC_BUSY_THRESHOLD },
  	{ "gc_idle_threshold",	KW_GC_IDLE_THRESHOLD },
 	{ "time_reopen",	KW_TIME_REOPEN },
 	{ "time_reap",          KW_TIME_REAP },
 	{ "time_sleep",         KW_TIME_SLEEP },
 	{ "follow_freq",	KW_FOLLOW_FREQ },
 	{ "remove_if_older",	KW_REMOVE_IF_OLDER },
 	{ "file_template",	KW_FILE_TEMPLATE },
 	{ "proto_template",	KW_PROTO_TEMPLATE },
 	
	{ "log_fifo_size",	KW_LOG_FIFO_SIZE },
	{ "log_fetch_limit",	KW_LOG_FETCH_LIMIT },
	{ "log_iw_size",	KW_LOG_IW_SIZE },
	{ "log_msg_size",	KW_LOG_MSG_SIZE },
	{ "log_prefix",		KW_LOG_PREFIX },
	
        { "create_dirs",        KW_CREATE_DIRS },
        { "optional",           KW_OPTIONAL },
 	{ "localip",		KW_LOCALIP },
	{ "ip",			KW_IP },
	{ "localport",		KW_LOCALPORT },
	{ "port",		KW_PORT },
	{ "destport",		KW_DESTPORT },
        { "ip_ttl",             KW_IP_TTL },
        { "ip_tos",             KW_IP_TOS },
        { "so_broadcast",       KW_SO_BROADCAST },
        { "so_rcvbuf",          KW_SO_RCVBUF },
        { "so_sndbuf",          KW_SO_SNDBUF },

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
	{ "required",		KW_REQUIRED },
	{ "allow",		KW_ALLOW },
	{ "deny",		KW_DENY },
	{ "compress",		KW_COMPRESS },
	{ "persist_only",       KW_PERSIST_ONLY },
	{ "dns_cache_hosts",    KW_DNS_CACHE_HOSTS },
	{ "dns_cache",		KW_DNS_CACHE },
	{ "dns_cache_size",	KW_DNS_CACHE_SIZE },
	{ "dns_cache_expire",	KW_DNS_CACHE_EXPIRE },
	{ "dns_cache_expire_failed", KW_DNS_CACHE_EXPIRE_FAILED },


	/* filter items */
        { "or",                 KW_OR },
	{ "and",                KW_AND },
        { "not",                KW_NOT },
	{ "level",              KW_LEVEL },
	{ "priority",           KW_LEVEL },
	{ "facility",           KW_FACILITY },
	{ "program",		KW_PROGRAM },
        { "host",               KW_HOST },
        { "match",		KW_MATCH },
        { "netmask",		KW_NETMASK },

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
\n			   { linenum++; }
{white}+		   ;
\.\.                       { return DOTDOT; }
0x{digit}+ 		   { yylval.num = strtol(yytext, NULL, 16); return NUMBER; }
0{digit}+		   { yylval.num = strtol(yytext, NULL, 8); return NUMBER; }
(-|\+)?{digit}+            { yylval.num = strtol(yytext, NULL, 10); return NUMBER; }
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
lex_init(FILE *file)
{
  yyrestart(file);
  linenum = 1;
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
        return keywords[i].kw_token;
    }
  yylval.cptr = strdup(token);
  return IDENTIFIER;
}

int
lookup_parse_flag(char *flag)
{
  if (strcmp(flag, "no_parse") == 0 || strcmp(flag, "no-parse") == 0)
    return LRO_NOPARSE;
  if (strcmp(flag, "kernel") == 0)
    return LRO_KERNEL;
  msg_notice("Unknown parse flag", evt_tag_str("flag", flag), NULL);
  return 0;
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
