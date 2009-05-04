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
#include <sys/stat.h>

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
	{ "include",            KW_INCLUDE },

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
        { "local_time_zone",    KW_LOCAL_TIME_ZONE },
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
	{ "default_level",      KW_DEFAULT_LEVEL },
	{ "default_priority",   KW_DEFAULT_LEVEL },
	{ "default_facility",   KW_DEFAULT_FACILITY },
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

typedef struct _CfgIncludeLevel
{
  GSList *files;
  gchar *current_file;
  gint linenum;
  struct yy_buffer_state *yybuf;
} CfgIncludeLevel;

#define MAX_INCLUDE_DEPTH 16

static CfgIncludeLevel include_stack[MAX_INCLUDE_DEPTH];
static gint include_depth = 0;


int lex_filter_params = 0;
char buf[MAX_REGEXP_LEN];
char *str;

static int check_reserved_words(char *token);
static void append_string(int length, char *str);
static void append_char(char c);
static gboolean cfg_start_next_include(gboolean first_on_this_level);

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
\r?\n			   { include_stack[include_depth].linenum++; }
{white}+		   ;
\.\.                       { return LL_DOTDOT; }
(-|\+)?{digit}+\.{digit}+  { yylval.fnum = strtod(yytext, NULL); return LL_FLOAT; }
0x{digit}+ 		   { yylval.num = strtoll(yytext, NULL, 16); return LL_NUMBER; }
0{digit}+		   { yylval.num = strtoll(yytext, NULL, 8); return LL_NUMBER; }
(-|\+)?{digit}+            { yylval.num = strtoll(yytext, NULL, 10); return LL_NUMBER; }
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
				*str = 0;
				/* yy_push_state(string);*/
				BEGIN(string);
			   }
\'			   {
				str = buf;
				*str = 0;
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
				return LL_STRING; 
		           }
<string>[^"\\]+		   { append_string(strlen(yytext), yytext); }
<qstring>[^']+		   { append_string(strlen(yytext), yytext); }
<qstring>\'		   { 
				BEGIN(INITIAL);
				yylval.cptr = strdup(buf);
				return LL_STRING;
			   }


<INITIAL><<EOF>>           { if (!cfg_start_next_include(FALSE)) yyterminate(); }

%%


static int 
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
          yylval.token = keywords[i].kw_token;
          return keywords[i].kw_token;
        }
    }
  yylval.cptr = strdup(token);
  return LL_IDENTIFIER;
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

const gchar *
cfg_lex_get_current_file(void)
{
  CfgIncludeLevel *level = &include_stack[include_depth];

  return level->current_file;
}

gint
cfg_lex_get_current_lineno(void)
{
  CfgIncludeLevel *level = &include_stack[include_depth];

  return level->linenum;
}

char *
cfg_lex_get_keyword_string(int kw)
{
  int i;
  for (i = 0; i < (sizeof(keywords) / sizeof(struct keyword)); i++)
    {
      if (keywords[i].kw_token == kw)
        {
          msg_warning("WARNING: Your configuration uses a newly introduced reserved word as identifier, please use a different name",
                      evt_tag_str("keyword", keywords[i].kw_name),
                      evt_tag_str("filename", cfg_lex_get_current_file()),
                      evt_tag_int("line", cfg_lex_get_current_lineno()),
                      NULL);
          return strdup(keywords[i].kw_name);
        }
    }
  g_assert_not_reached();
}



static gboolean
cfg_start_next_include(gboolean first_on_this_level)
{
  FILE *include_file;
  CfgIncludeLevel *level = &include_stack[include_depth];
  gchar *filename;
  struct yy_buffer_state *yybuf;
  
  g_assert(level->yybuf == NULL);
  
  if (include_depth == 0)
    {
      return FALSE;
    }

  if (!first_on_this_level)
    {
      msg_debug("Finishing include file",
                evt_tag_str("filename", level->current_file),
                evt_tag_int("depth", include_depth),
                NULL);
    }
  
  if (!level->files)
    {
      yybuf = YY_CURRENT_BUFFER;
      g_free(level->current_file);
      fclose(yybuf->yy_input_file);
      level->current_file = NULL;
      include_depth--;
      yy_switch_to_buffer(include_stack[include_depth].yybuf);
      include_stack[include_depth].yybuf = NULL;
      if (!first_on_this_level)
        yy_delete_buffer(yybuf);
        
      return TRUE;
    }
    
  filename = (gchar *) level->files->data;
  level->files = g_slist_delete_link(level->files, level->files);
  
  include_file = fopen(filename, "r");
  if (!include_file)
    {
      msg_error("Error opening include file",
                evt_tag_str("filename", filename),
                evt_tag_int("depth", include_depth),
                NULL);
      g_free(filename);
      return FALSE;
    }
  msg_debug("Starting to read include file",
            evt_tag_str("filename", filename),
            evt_tag_int("depth", include_depth),
            NULL);
  yybuf = YY_CURRENT_BUFFER;
  if (level->current_file)
    {
      g_free(level->current_file);
      if (yybuf)
        fclose(yybuf->yy_input_file);
    }
  level->current_file = filename;
  level->linenum = 1;
  
  yy_switch_to_buffer(yy_create_buffer(include_file, YY_BUF_SIZE));
  if (!first_on_this_level)
    yy_delete_buffer(yybuf);
  return TRUE;
}

gboolean
cfg_lex_process_include(const gchar *filename)
{
  struct stat st;
  gchar buf[1024];
  CfgIncludeLevel *level;
  
  if (include_depth >= MAX_INCLUDE_DEPTH - 1)
    {
      msg_error("Include file depth is too deep, increase MAX_INCLUDE_DEPTH and recompile",
                evt_tag_str("filename", filename),
                evt_tag_int("depth", include_depth),
                NULL);
      return FALSE;
    }
  
  if (filename[0] != '/')
    {
      g_snprintf(buf, sizeof(buf), "%s/%s", PATH_SYSCONFDIR, filename);
      filename = buf;
    }
  
  if (stat(filename, &st) < 0)
    {
      msg_error("Include file/directory not found", 
                evt_tag_str("filename", filename),
                evt_tag_errno("error", errno),
                NULL);
      return FALSE;
    }
  include_stack[include_depth].yybuf = YY_CURRENT_BUFFER;
  include_depth++;
  level = &include_stack[include_depth];
  if (S_ISDIR(st.st_mode))
    {
      GDir *dir;
      GError *error = NULL;
      const gchar *entry;
      
      dir = g_dir_open(filename, 0, &error);
      if (!dir)
        {
          msg_error("Error opening directory for reading",
                evt_tag_str("filename", filename),
                evt_tag_str("error", error->message),
                NULL);
          goto drop_level;
        }
      while ((entry = g_dir_read_name(dir)))
        {
          const gchar *p;
          
          for (p = entry; *p; p++)
            {
              if (!((*p >= 'a' && *p <= 'z') ||
                   (*p >= 'A' && *p <= 'Z') ||
                   (*p >= '0' && *p <= '9') ||
                   (*p == '_') || (*p == '-') || (*p == '.')))
                {
                  msg_debug("Skipping include file, does not match pattern [\\-_a-zA-Z0-9]+",
                            evt_tag_str("filename", entry),
                            NULL);
                  p = NULL;
                  break;
                }
            }
          if (p)
            {
              gchar *full_filename = g_build_filename(filename, entry, NULL);
              if (stat(full_filename, &st) < 0 || S_ISDIR(st.st_mode))
                {
                  msg_debug("Skipping include file as it is a directory",
                            evt_tag_str("filename", entry),
                            NULL);
                  g_free(full_filename);
                  continue;
                }
              level->files = g_slist_insert_sorted(level->files, full_filename, (GCompareFunc) strcmp);
              msg_debug("Adding include file",
                        evt_tag_str("filename", entry),
                        NULL);
            }
        }
      g_dir_close(dir);
      if (!level->files)
        {
          /* no include files in the specified directory */
          msg_debug("No files in this include directory",
                    evt_tag_str("dir", filename),
                    NULL);
          include_depth--;
          include_stack[include_depth].yybuf = NULL;
          return TRUE;
        }
    }
  else
    {
      g_assert(level->files == NULL);
      level->files = g_slist_prepend(level->files, g_strdup(filename));
    }
  return cfg_start_next_include(TRUE);
 drop_level:
  g_slist_foreach(level->files, (GFunc) g_free, NULL);
  g_slist_free(level->files);
  level->files = NULL;
  return FALSE;
}

int 
cfg_lex_init(FILE *file, gint init_line_num)
{
  CfgIncludeLevel *level;
  
  yyrestart(file);
  level = &include_stack[0];
  level->current_file = g_strdup(configuration->filename);
  level->linenum = init_line_num;
  return 0;
}

void
cfg_lex_deinit(void)
{
  gint i;
  
  for (i = 0; i < include_depth; i++)
    {
      CfgIncludeLevel *level = &include_stack[i];

      if (level->current_file)
        g_free(level->current_file);
      
      g_slist_foreach(level->files, (GFunc) g_free, NULL);
      g_slist_free(level->files);
      level->files = NULL;
      if (level->yybuf)
        yy_delete_buffer(level->yybuf);
    }
}
