%code requires {

/* YYSTYPE and YYLTYPE is defined by the lexer */
#include "cfg-lexer.h"

}

%name-prefix "main_"
%lex-param {CfgLexer *lexer}
%parse-param {CfgLexer *lexer}
%parse-param {gpointer *dummy}

/* START_DECLS */

%require "2.4.1"
%locations
%define api.pure
%pure-parser
%error-verbose

%code {

# define YYLLOC_DEFAULT(Current, Rhs, N)                                \
  do {                                                                  \
    (Current).level = YYRHSLOC(Rhs, 0).level;                           \
    if (YYID (N))                                                       \
      {                                                                 \
        (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;          \
        (Current).first_column = YYRHSLOC (Rhs, 1).first_column;        \
        (Current).last_line    = YYRHSLOC (Rhs, N).last_line;           \
        (Current).last_column  = YYRHSLOC (Rhs, N).last_column;         \
      }                                                                 \
    else                                                                \
      {                                                                 \
        (Current).first_line   = (Current).last_line   =                \
          YYRHSLOC (Rhs, 0).last_line;                                  \
        (Current).first_column = (Current).last_column =                \
          YYRHSLOC (Rhs, 0).last_column;                                \
      }                                                                 \
  } while (YYID (0))

#define CHECK_ERROR(val, token, errorfmt, ...) do {                     \
    if (!(val))                                                         \
      {                                                                 \
        if (errorfmt)                                                   \
          {                                                             \
            gchar __buf[256];                                           \
            g_snprintf(__buf, sizeof(__buf), errorfmt, ## __VA_ARGS__); \
            yyerror(& (token), lexer, NULL, __buf);                     \
          }                                                             \
        YYERROR;                                                        \
      }                                                                 \
  } while (0)


}

/* plugin types, must be equal to the numerical values of the plugin type in plugin.h */

%token LL_CONTEXT_ROOT                1
%token LL_CONTEXT_DESTINATION         2
%token LL_CONTEXT_SOURCE              3
%token LL_CONTEXT_PARSER              4
%token LL_CONTEXT_REWRITE             5
%token LL_CONTEXT_FILTER              6
%token LL_CONTEXT_BLOCK_DEF           7
%token LL_CONTEXT_BLOCK_REF           8
%token LL_CONTEXT_BLOCK_CONTENT       9
%token LL_CONTEXT_PRAGMA              10

/* statements */
%token KW_SOURCE                      10000
%token KW_FILTER                      10001
%token KW_PARSER                      10002
%token KW_DESTINATION                 10003
%token KW_LOG                         10004
%token KW_OPTIONS                     10005
%token KW_INCLUDE                     10006
%token KW_BLOCK                       10007

/* source & destination items */
%token KW_INTERNAL                    10010
%token KW_FILE                        10011
%token KW_PIPE                        10012
%token KW_USERTTY                     10019

%token KW_SQL                         10030
%token KW_TYPE                        10031
%token KW_COLUMNS                     10032
%token KW_INDEXES                     10033
%token KW_VALUES                      10034
%token KW_PASSWORD                    10035
%token KW_DATABASE                    10036
%token KW_USERNAME                    10037
%token KW_TABLE                       10038
%token KW_ENCODING                    10039
%token KW_SESSION_STATEMENTS          10040

%token KW_DELIMITERS                  10050
%token KW_QUOTES                      10051
%token KW_QUOTE_PAIRS                 10052
%token KW_NULL                        10053

%token KW_SYSLOG                      10060


/* option items */
%token KW_MARK_FREQ                   10071
%token KW_STATS_FREQ                  10072
%token KW_STATS_LEVEL                 10073
%token KW_FLUSH_LINES                 10074
%token KW_SUPPRESS                    10075
%token KW_FLUSH_TIMEOUT               10076
%token KW_LOG_MSG_SIZE                10077
%token KW_FILE_TEMPLATE               10078
%token KW_PROTO_TEMPLATE              10079

%token KW_CHAIN_HOSTNAMES             10090
%token KW_NORMALIZE_HOSTNAMES         10091
%token KW_KEEP_HOSTNAME               10092
%token KW_CHECK_HOSTNAME              10093
%token KW_BAD_HOSTNAME                10094

%token KW_KEEP_TIMESTAMP              10100

%token KW_USE_DNS                     10110
%token KW_USE_FQDN                    10111

%token KW_DNS_CACHE                   10120
%token KW_DNS_CACHE_SIZE              10121

%token KW_DNS_CACHE_EXPIRE            10130
%token KW_DNS_CACHE_EXPIRE_FAILED     10131
%token KW_DNS_CACHE_HOSTS             10132

%token KW_PERSIST_ONLY                10140

%token KW_TZ_CONVERT                  10150
%token KW_TS_FORMAT                   10151
%token KW_FRAC_DIGITS                 10152

%token KW_LOG_FIFO_SIZE               10160
%token KW_LOG_FETCH_LIMIT             10162
%token KW_LOG_IW_SIZE                 10163
%token KW_LOG_PREFIX                  10164
%token KW_PROGRAM_OVERRIDE            10165
%token KW_HOST_OVERRIDE               10166

%token KW_THROTTLE                    10170

/* log statement options */
%token KW_FLAGS                       10190

/* reader options */
%token KW_PAD_SIZE                    10200
%token KW_TIME_ZONE                   10201
%token KW_RECV_TIME_ZONE              10202
%token KW_SEND_TIME_ZONE              10203
%token KW_LOCAL_TIME_ZONE             10204

/* timers */
%token KW_TIME_REOPEN                 10210
%token KW_TIME_REAP                   10211
%token KW_TIME_SLEEP                  10212

/* destination options */
%token KW_TMPL_ESCAPE                 10220

/* driver specific options */
%token KW_OPTIONAL                    10230

/* file related options */
%token KW_CREATE_DIRS                 10240

%token KW_OWNER                       10250
%token KW_GROUP                       10251
%token KW_PERM                        10252

%token KW_DIR_OWNER                   10260
%token KW_DIR_GROUP                   10261
%token KW_DIR_PERM                    10262

%token KW_TEMPLATE                    10270
%token KW_TEMPLATE_ESCAPE             10271

%token KW_DEFAULT_FACILITY            10300
%token KW_DEFAULT_LEVEL               10301

%token KW_PORT                        10323
/* misc options */

%token KW_USE_TIME_RECVD              10340

/* filter items*/
%token KW_FACILITY                    10350
%token KW_LEVEL                       10351
%token KW_HOST                        10352
%token KW_MATCH                       10353
%token KW_MESSAGE                     10354
%token KW_NETMASK                     10355
%token KW_TAGS                        10356

/* parser items */

%token KW_CSV_PARSER                  10360
%token KW_VALUE                       10361
%token KW_DB_PARSER                   10362

/* rewrite items */

%token KW_REWRITE                     10370
%token KW_SET                         10371
%token KW_SUBST                       10372

/* yes/no switches */

%token KW_YES                         10380
%token KW_NO                          10381

%token KW_IFDEF                       10410
%token KW_ENDIF                       10411

%token LL_DOTDOT                      10420

%token <cptr> LL_IDENTIFIER           10421
%token <num>  LL_NUMBER               10422
%token <fnum> LL_FLOAT                10423
%token <cptr> LL_STRING               10424
%token <token> LL_TOKEN               10425
%token <cptr> LL_BLOCK                10426
%token LL_PRAGMA                      10427
%token LL_EOL                         10428
%token LL_ERROR                       10429

/* END_DECLS */

%code {

#include "cfg-parser.h"
#include "cfg.h"
#include "sgroup.h"
#include "dgroup.h"
#include "center.h"
#include "filter.h"
#include "templates.h"
#include "logreader.h"
#include "logparser.h"
#include "logrewrite.h"
#include "filter-expr-parser.h"
#include "rewrite-expr-parser.h"
#include "block-ref-parser.h"
#include "parser-expr-parser.h"
#include "plugin.h"


#include "afinter.h"
#include "afuser.h"
#include "logwriter.h"

#include "messages.h"

#include "syslog-names.h"

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cfg-grammar.h"

LogDriver *last_driver;
LogReaderOptions *last_reader_options;
LogWriterOptions *last_writer_options;
LogTemplate *last_template;
GList *last_rewrite_expr;
GList *last_parser_expr;
FilterExprNode *last_filter_expr;
CfgArgs *last_block_args;

}


%type	<ptr> source_stmt
%type	<ptr> filter_stmt
%type   <ptr> parser_stmt
%type   <ptr> rewrite_stmt
%type	<ptr> dest_stmt
%type	<ptr> log_stmt
%type	<ptr> options_stmt
%type	<ptr> template_stmt

%type	<ptr> source_items
%type	<ptr> source_item
%type   <ptr> source_afinter
%type   <ptr> source_plugin

%type	<ptr> dest_items
%type	<ptr> dest_item
%type   <ptr> dest_afuser
%type   <ptr> dest_plugin

%type	<ptr> log_items
%type	<ptr> log_item

%type   <ptr> log_forks
%type   <ptr> log_fork

%type	<num> log_flags
%type   <num> log_flags_items

%type	<ptr> options_items
%type	<ptr> options_item

/* START_DECLS */

%type	<num> yesno
%type   <num> dnsmode
%type   <num> regexp_option_flags
%type	<num> dest_writer_options_flags

%type	<cptr> string
%type	<cptr> string_or_number
%type   <ptr> string_list
%type   <ptr> string_list_build
%type   <num> facility_string
%type   <num> level_string

/* END_DECLS */


%%

start
        : stmts
	;

stmts
        : stmt ';' stmts
	|
	;

stmt
        : KW_SOURCE source_stmt			{ cfg_add_source(configuration, $2); }
	| KW_DESTINATION dest_stmt		{ cfg_add_dest(configuration, $2); }
	| KW_LOG log_stmt			{ cfg_add_connection(configuration, $2); }
	| KW_FILTER filter_stmt			{ cfg_add_filter(configuration, $2); }
	| KW_PARSER parser_stmt                 { cfg_add_parser(configuration, $2); }
        | KW_REWRITE rewrite_stmt               { cfg_add_rewrite(configuration, $2); }
	| KW_TEMPLATE template_stmt		{ cfg_add_template(configuration, $2); }
	| KW_OPTIONS options_stmt		{  }
	| KW_BLOCK block_stmt                   {  }
	;

source_stmt
	: string
          { cfg_lexer_push_context(lexer, LL_CONTEXT_SOURCE, NULL, "source"); }
          '{' source_items '}'
          { cfg_lexer_pop_context(lexer); }     { $$ = log_source_group_new($1, $4); free($1); }
	;

filter_stmt
	: string '{'
	  {
	    last_filter_expr = NULL;
	    CHECK_ERROR(cfg_parser_parse(&filter_expr_parser, lexer, (gpointer *) &last_filter_expr), @1, NULL);
	  }
          '}'                               { $$ = log_filter_rule_new($1, last_filter_expr); free($1); }
	;
	
parser_stmt
        : string '{'
          {
            CHECK_ERROR(cfg_parser_parse(&parser_expr_parser, lexer, (gpointer *) &last_parser_expr), @1, NULL);
          } '}'	                                { $$ = log_parser_rule_new($1, last_parser_expr); free($1); }

rewrite_stmt
        : string '{'
          {
            CHECK_ERROR(cfg_parser_parse(&rewrite_expr_parser, lexer, (gpointer *) &last_rewrite_expr), @1, NULL);
          } '}'                                 { $$ = log_rewrite_rule_new($1, last_rewrite_expr); free($1); }

dest_stmt
        : string
          { cfg_lexer_push_context(lexer, LL_CONTEXT_DESTINATION, NULL, "destination"); }
          '{' dest_items '}'
          { cfg_lexer_pop_context(lexer); }         { $$ = log_dest_group_new($1, $4); free($1); }
	;

log_stmt
        : '{' log_items log_forks log_flags '}'		{ LogPipeItem *pi = log_pipe_item_append_tail($2, $3); $$ = log_connection_new(pi, $4); }
	;
	
block_stmt
        : { cfg_lexer_push_context(lexer, LL_CONTEXT_BLOCK_DEF, block_def_keywords, "block definition"); }
          LL_IDENTIFIER LL_IDENTIFIER
          '(' { last_block_args = cfg_args_new(); } block_args ')'
          { cfg_lexer_push_context(lexer, LL_CONTEXT_BLOCK_CONTENT, NULL, "block content"); }
          LL_BLOCK
                                                          {
                                                            CfgBlock *block;

                                                            /* block content */
                                                            cfg_lexer_pop_context(lexer);
                                                            /* block definition */
                                                            cfg_lexer_pop_context(lexer);

                                                            block = cfg_block_new($9, last_block_args);
                                                            cfg_lexer_register_block_generator(lexer, cfg_lexer_lookup_context_type_by_name($2), $3, cfg_block_generate, block, (GDestroyNotify) cfg_block_free);
                                                            free($9);
                                                            last_block_args = NULL;
                                                          }
        ;

block_args
        : block_arg block_args
        |
        ;

block_arg
        : LL_IDENTIFIER '(' string_or_number ')'          { cfg_args_set(last_block_args, $1, $3); free($1); free($3); }
        | LL_IDENTIFIER '(' ')'                           {}
        ;

log_items
	: log_item ';' log_items		{ log_pipe_item_append($1, $3); $$ = $1; }
	|					{ $$ = NULL; }
	;

log_item
	: KW_SOURCE '(' string ')'		{ $$ = log_pipe_item_new(EP_SOURCE, $3); free($3); }
	| KW_FILTER '(' string ')'		{ $$ = log_pipe_item_new(EP_FILTER, $3); free($3); }
        | KW_PARSER '(' string ')'              { $$ = log_pipe_item_new(EP_PARSER, $3); free($3); }
        | KW_REWRITE '(' string ')'              { $$ = log_pipe_item_new(EP_REWRITE, $3); free($3); }
	| KW_DESTINATION '(' string ')'		{ $$ = log_pipe_item_new(EP_DESTINATION, $3); free($3); }
	;

log_forks
        : log_fork log_forks			{ log_pipe_item_append($1, $2); $$ = $1; }
        |                                       { $$ = NULL; }
        ;

log_fork
        : KW_LOG '{' log_items log_forks log_flags '}' ';' { LogPipeItem *pi = log_pipe_item_append_tail($3, $4); $$ = log_pipe_item_new_ref(EP_PIPE, log_connection_new(pi, $5)); }
        ;

log_flags
	: KW_FLAGS '(' log_flags_items ')' ';'	{ $$ = $3; }
	|					{ $$ = 0; }
	;


log_flags_items
	: string log_flags_items		{ $$ = log_connection_lookup_flag($1) | $2; free($1); }
	|					{ $$ = 0; }
	;


options_stmt
        : '{' options_items '}'			{ $$ = NULL; }
	;
	
template_stmt
	: string
	  {
	    last_template = log_template_new($1, NULL);
	    free($1);
	  }
	  '{' template_items '}'		{ $$ = last_template;  }
	;
	
template_items
	: template_item ';' template_items
	|
	;

template_item
	: KW_TEMPLATE '(' string ')'		{
                                                  GError *error = NULL;

                                                  last_template->template = g_strdup($3); free($3);
                                                  CHECK_ERROR(log_template_compile(last_template, &error), @3, "Error compiling template (%s)", error->message);
                                                }
	| KW_TEMPLATE_ESCAPE '(' yesno ')'	{ log_template_set_escape(last_template, $3); }
	;


source_items
        : source_item ';' source_items		{ if ($1) {log_drv_append($1, $3); log_drv_unref($3); $$ = $1; } else { YYERROR; } }
        | ';' source_items                      { $$ = $2; }
	|					{ $$ = NULL; }
	;

source_item
  	: source_afinter			{ $$ = $1; }
        | source_plugin                         { $$ = $1; }
	;

source_plugin
        : LL_IDENTIFIER
          {
            Plugin *p;
            gint context = LL_CONTEXT_SOURCE;

            p = plugin_find(configuration, context, $1);
            CHECK_ERROR(p, @1, "%s plugin %s not found", cfg_lexer_lookup_context_name_by_type(context), $1);

            last_driver = (LogDriver *) plugin_new_instance(configuration, p, &@1);
            free($1);
            if (!last_driver)
              {
                YYERROR;
              }
            $$ = last_driver;
          }
        ;

source_afinter
	: KW_INTERNAL '(' ')'			{ $$ = afinter_sd_new(); }
	;

dest_items
	: dest_item ';' dest_items		{ log_drv_append($1, $3); log_drv_unref($3); $$ = $1; }
        | ';' dest_items                        { $$ = $2; }
	|					{ $$ = NULL; }
	;

dest_item
        : dest_afuser				{ $$ = $1; }
        | dest_plugin                           { $$ = $1; }
	;

dest_plugin
        : LL_IDENTIFIER
          {
            Plugin *p;
            gint context = LL_CONTEXT_DESTINATION;

            p = plugin_find(configuration, context, $1);
            CHECK_ERROR(p, @1, "%s plugin %s not found", cfg_lexer_lookup_context_name_by_type(context), $1);

            last_driver = (LogDriver *) plugin_new_instance(configuration, p, &@1);
            free($1);
            if (!last_driver)
              {
                YYERROR;
              }
            $$ = last_driver;
          }
        ;


dest_afuser
	: KW_USERTTY '(' string ')'		{ $$ = afuser_dd_new($3); free($3); }
	;

options_items
	: options_item ';' options_items	{ $$ = $1; }
	|					{ $$ = NULL; }
	;

options_item
	: KW_MARK_FREQ '(' LL_NUMBER ')'		{ configuration->mark_freq = $3; }
	| KW_STATS_FREQ '(' LL_NUMBER ')'          { configuration->stats_freq = $3; }
	| KW_STATS_LEVEL '(' LL_NUMBER ')'         { configuration->stats_level = $3; }
	| KW_FLUSH_LINES '(' LL_NUMBER ')'		{ configuration->flush_lines = $3; }
	| KW_FLUSH_TIMEOUT '(' LL_NUMBER ')'	{ configuration->flush_timeout = $3; }
	| KW_CHAIN_HOSTNAMES '(' yesno ')'	{ configuration->chain_hostnames = $3; }
	| KW_NORMALIZE_HOSTNAMES '(' yesno ')'	{ configuration->normalize_hostnames = $3; }
	| KW_KEEP_HOSTNAME '(' yesno ')'	{ configuration->keep_hostname = $3; }
	| KW_CHECK_HOSTNAME '(' yesno ')'	{ configuration->check_hostname = $3; }
	| KW_BAD_HOSTNAME '(' string ')'	{ cfg_bad_hostname_set(configuration, $3); free($3); }
	| KW_USE_TIME_RECVD '(' yesno ')'	{ configuration->use_time_recvd = $3; }
	| KW_USE_FQDN '(' yesno ')'		{ configuration->use_fqdn = $3; }
	| KW_USE_DNS '(' dnsmode ')'		{ configuration->use_dns = $3; }
	| KW_TIME_REOPEN '(' LL_NUMBER ')'		{ configuration->time_reopen = $3; }
	| KW_TIME_REAP '(' LL_NUMBER ')'		{ configuration->time_reap = $3; }
	| KW_TIME_SLEEP '(' LL_NUMBER ')'		
		{
		  configuration->time_sleep = $3;
		  if ($3 > 500)
		    {
		      msg_notice("The value specified for time_sleep is too large", evt_tag_int("time_sleep", $3), NULL);
		      configuration->time_sleep = 500;
		    }
		}
	| KW_LOG_FIFO_SIZE '(' LL_NUMBER ')'	{ configuration->log_fifo_size = $3; }
	| KW_LOG_IW_SIZE '(' LL_NUMBER ')'		{ configuration->log_iw_size = $3; }
	| KW_LOG_FETCH_LIMIT '(' LL_NUMBER ')'	{ configuration->log_fetch_limit = $3; }
	| KW_LOG_MSG_SIZE '(' LL_NUMBER ')'	{ configuration->log_msg_size = $3; }
	| KW_KEEP_TIMESTAMP '(' yesno ')'	{ configuration->keep_timestamp = $3; }
	| KW_TS_FORMAT '(' string ')'		{ configuration->ts_format = cfg_ts_format_value($3); free($3); }
	| KW_FRAC_DIGITS '(' LL_NUMBER ')'		{ configuration->frac_digits = $3; }
	| KW_CREATE_DIRS '(' yesno ')'		{ configuration->create_dirs = $3; }
	| KW_OWNER '(' string_or_number ')'	{ cfg_file_owner_set(configuration, $3); free($3); }
	| KW_OWNER '(' ')'	                { cfg_file_owner_set(configuration, "-2"); }
	| KW_GROUP '(' string_or_number ')'	{ cfg_file_group_set(configuration, $3); free($3); }
	| KW_GROUP '(' ')'                    	{ cfg_file_group_set(configuration, "-2"); }
	| KW_PERM '(' LL_NUMBER ')'		{ cfg_file_perm_set(configuration, $3); }
	| KW_PERM '(' ')'		        { cfg_file_perm_set(configuration, -2); }
	| KW_DIR_OWNER '(' string_or_number ')'	{ cfg_dir_owner_set(configuration, $3); free($3); }
	| KW_DIR_OWNER '('  ')'	                { cfg_dir_owner_set(configuration, "-2"); }
	| KW_DIR_GROUP '(' string_or_number ')'	{ cfg_dir_group_set(configuration, $3); free($3); }
	| KW_DIR_GROUP '('  ')'	                { cfg_dir_group_set(configuration, "-2"); }
	| KW_DIR_PERM '(' LL_NUMBER ')'		{ cfg_dir_perm_set(configuration, $3); }
	| KW_DIR_PERM '('  ')'		        { cfg_dir_perm_set(configuration, -2); }
	| KW_DNS_CACHE '(' yesno ')' 		{ configuration->use_dns_cache = $3; }
	| KW_DNS_CACHE_SIZE '(' LL_NUMBER ')'	{ configuration->dns_cache_size = $3; }
	| KW_DNS_CACHE_EXPIRE '(' LL_NUMBER ')'	{ configuration->dns_cache_expire = $3; }
	| KW_DNS_CACHE_EXPIRE_FAILED '(' LL_NUMBER ')'
	  			{ configuration->dns_cache_expire_failed = $3; }
	| KW_DNS_CACHE_HOSTS '(' string ')'     { configuration->dns_cache_hosts = g_strdup($3); free($3); }
	| KW_FILE_TEMPLATE '(' string ')'	{ configuration->file_template_name = g_strdup($3); free($3); }
	| KW_PROTO_TEMPLATE '(' string ')'	{ configuration->proto_template_name = g_strdup($3); free($3); }
	| KW_RECV_TIME_ZONE '(' string ')'      { configuration->recv_time_zone = g_strdup($3); free($3); }
	| KW_SEND_TIME_ZONE '(' string ')'      { configuration->send_time_zone = g_strdup($3); free($3); }
	| KW_LOCAL_TIME_ZONE '(' string ')'     { configuration->local_time_zone = g_strdup($3); free($3); }
	;

/* START_RULES */

string
	: LL_IDENTIFIER
	| LL_STRING
	;

yesno
	: KW_YES				{ $$ = 1; }
	| KW_NO					{ $$ = 0; }
	| LL_NUMBER				{ $$ = $1; }
	;

dnsmode
	: yesno					{ $$ = $1; }
	| KW_PERSIST_ONLY                       { $$ = 2; }
	;

string_or_number
        : string                                { $$ = $1; }
        | LL_NUMBER                             { char buf[32]; snprintf(buf, sizeof(buf), "%" G_GINT64_FORMAT, $1); $$ = strdup(buf); }
        | LL_FLOAT                              { char buf[32]; snprintf(buf, sizeof(buf), "%.2f" , $1); $$ = strdup(buf); }
        ;

string_list
        : string_list_build                     { $$ = g_list_reverse($1); }
        ;

string_list_build
        : string string_list_build		{ $$ = g_list_append($2, g_strdup($1)); free($1); }
        |					{ $$ = NULL; }
        ;

level_string
        : string
	  {
	    /* return the numeric value of the "level" */
	    int n = syslog_name_lookup_level_by_name($1);
	    CHECK_ERROR((n != -1), @1, "Unknown priority level\"%s\"", $1);
	    free($1);
            $$ = n;
	  }
        ;

facility_string
        : string
          {
            /* return the numeric value of facility */
	    int n = syslog_name_lookup_facility_by_name($1);
	    CHECK_ERROR((n != -1), @1, "Unknown facility \"%s\"", $1);
	    free($1);
	    $$ = n;
	  }
        | KW_SYSLOG 				{ $$ = LOG_SYSLOG; }
        ;

regexp_option_flags
        : string regexp_option_flags            { $$ = log_matcher_lookup_flag($1) | $2; free($1); }
        |                                       { $$ = 0; }
        ;


source_reader_options
	: source_reader_option source_reader_options
	|
	;

source_reader_option
	: KW_LOG_IW_SIZE '(' LL_NUMBER ')'		{ last_reader_options->super.init_window_size = $3; }
	| KW_CHAIN_HOSTNAMES '(' yesno ')'	{ last_reader_options->super.chain_hostnames = $3; }
	| KW_NORMALIZE_HOSTNAMES '(' yesno ')'	{ last_reader_options->super.normalize_hostnames = $3; }
	| KW_KEEP_HOSTNAME '(' yesno ')'	{ last_reader_options->super.keep_hostname = $3; }
        | KW_USE_FQDN '(' yesno ')'             { last_reader_options->super.use_fqdn = $3; }
        | KW_USE_DNS '(' dnsmode ')'            { last_reader_options->super.use_dns = $3; }
	| KW_DNS_CACHE '(' yesno ')' 		{ last_reader_options->super.use_dns_cache = $3; }
	| KW_PROGRAM_OVERRIDE '(' string ')'	{ last_reader_options->super.program_override = g_strdup($3); free($3); }
	| KW_HOST_OVERRIDE '(' string ')'	{ last_reader_options->super.host_override = g_strdup($3); free($3); }
	| KW_LOG_PREFIX '(' string ')'	        { gchar *p = strrchr($3, ':'); if (p) *p = 0; last_reader_options->super.program_override = g_strdup($3); free($3); }
	| KW_TIME_ZONE '(' string ')'		{ last_reader_options->parse_options.recv_time_zone = g_strdup($3); free($3); }
	| KW_CHECK_HOSTNAME '(' yesno ')'	{ last_reader_options->check_hostname = $3; }
	| KW_FLAGS '(' source_reader_option_flags ')'
	| KW_LOG_MSG_SIZE '(' LL_NUMBER ')'	{ last_reader_options->msg_size = $3; }
	| KW_LOG_FETCH_LIMIT '(' LL_NUMBER ')'	{ last_reader_options->fetch_limit = $3; }
	| KW_PAD_SIZE '(' LL_NUMBER ')'		{ last_reader_options->padding = $3; }
	| KW_KEEP_TIMESTAMP '(' yesno ')'	{ last_reader_options->super.keep_timestamp = $3; }
        | KW_ENCODING '(' string ')'		{ last_reader_options->text_encoding = g_strdup($3); free($3); }
        | KW_TAGS '(' string_list ')'           { log_reader_options_set_tags(last_reader_options, $3); }
	| KW_DEFAULT_LEVEL '(' level_string ')'
	  {
	    if (last_reader_options->parse_options.default_pri == 0xFFFF)
	      last_reader_options->parse_options.default_pri = LOG_USER;
	    last_reader_options->parse_options.default_pri = (last_reader_options->parse_options.default_pri & ~7) | $3;
          }
	| KW_DEFAULT_FACILITY '(' facility_string ')'
	  {
	    if (last_reader_options->parse_options.default_pri == 0xFFFF)
	      last_reader_options->parse_options.default_pri = LOG_NOTICE;
	    last_reader_options->parse_options.default_pri = (last_reader_options->parse_options.default_pri & 7) | $3;
          }
	;

source_reader_option_flags
        : string source_reader_option_flags     { CHECK_ERROR(log_reader_options_process_flag(last_reader_options, $1), @1, "Unknown flag %s", $1); free($1); }
	|
	;

dest_writer_options
	: dest_writer_option dest_writer_options
	|
	;

dest_writer_option
	: KW_FLAGS '(' dest_writer_options_flags ')' { last_writer_options->options = $3; }
	| KW_LOG_FIFO_SIZE '(' LL_NUMBER ')'	{ last_writer_options->mem_fifo_size = $3; }
	| KW_FLUSH_LINES '(' LL_NUMBER ')'		{ last_writer_options->flush_lines = $3; }
	| KW_FLUSH_TIMEOUT '(' LL_NUMBER ')'	{ last_writer_options->flush_timeout = $3; }
        | KW_SUPPRESS '(' LL_NUMBER ')'            { last_writer_options->suppress = $3; }
	| KW_TEMPLATE '(' string ')'       	{
                                                  GError *error = NULL;

	                                          last_writer_options->template = cfg_check_inline_template(configuration, $3);
                                                  CHECK_ERROR(log_template_compile(last_writer_options->template, &error), @3, "Error compiling template (%s)", error->message);
	                                          free($3);
	                                        }
	| KW_TEMPLATE_ESCAPE '(' yesno ')'	{ log_writer_options_set_template_escape(last_writer_options, $3); }
	| KW_TIME_ZONE '(' string ')'           { last_writer_options->send_time_zone = g_strdup($3); free($3); }
	| KW_TS_FORMAT '(' string ')'		{ last_writer_options->ts_format = cfg_ts_format_value($3); free($3); }
	| KW_FRAC_DIGITS '(' LL_NUMBER ')'		{ last_writer_options->frac_digits = $3; }
	| KW_THROTTLE '(' LL_NUMBER ')'            { last_writer_options->throttle = $3; }
	;

dest_writer_options_flags
	: string dest_writer_options_flags      { $$ = log_writer_options_lookup_flag($1) | $2; free($1); }
	|					{ $$ = 0; }
	;

/* END_RULES */


%%

