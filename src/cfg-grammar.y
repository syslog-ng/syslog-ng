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

/* plugin types, must be equal to the numerical values of the plugin type in plugin.h */

%token LL_CONTEXT_DESTINATION         1
%token LL_CONTEXT_SOURCE              2
%token LL_CONTEXT_PARSER              3
%token LL_CONTEXT_REWRITE             4
%token LL_CONTEXT_FILTER              5

/* statements */
%token KW_SOURCE                      10000
%token KW_FILTER                      10001
%token KW_PARSER                      10002
%token KW_DESTINATION                 10003
%token KW_LOG                         10004
%token KW_OPTIONS                     10005
%token KW_INCLUDE                     10006

/* source & destination items */
%token KW_INTERNAL                    10010
%token KW_FILE                        10011
%token KW_PIPE                        10012
%token KW_UNIX_STREAM                 10013
%token KW_UNIX_DGRAM                  10014
%token KW_TCP                         10015
%token KW_UDP                         10016
%token KW_TCP6                        10017
%token KW_UDP6                        10018
%token KW_USERTTY                     10019
%token KW_DOOR                        10020
%token KW_SUN_STREAMS                 10021
%token KW_PROGRAM                     10022

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
%token KW_TRANSPORT                   10061

/* option items */
%token KW_FSYNC                       10070
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
%token KW_LOG_DISK_FIFO_SIZE          10161
%token KW_LOG_FETCH_LIMIT             10162
%token KW_LOG_IW_SIZE                 10163
%token KW_LOG_PREFIX                  10164
%token KW_PROGRAM_OVERRIDE            10165
%token KW_HOST_OVERRIDE               10166

%token KW_THROTTLE                    10170

/* SSL support */

%token KW_TLS                         10180
%token KW_PEER_VERIFY                 10181
%token KW_KEY_FILE                    10182
%token KW_CERT_FILE                   10183
%token KW_CA_DIR                      10184
%token KW_CRL_DIR                     10185
%token KW_TRUSTED_KEYS                10186
%token KW_TRUSTED_DN                  10187

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

%token KW_FOLLOW_FREQ                 10280

%token KW_OVERWRITE_IF_OLDER          10290

%token KW_DEFAULT_FACILITY            10300
%token KW_DEFAULT_LEVEL               10301

/* socket related options */

%token KW_KEEP_ALIVE                  10310
%token KW_MAX_CONNECTIONS             10311

%token KW_LOCALIP                     10320
%token KW_IP                          10321
%token KW_LOCALPORT                   10322
%token KW_PORT                        10323
%token KW_DESTPORT                    10324

%token KW_IP_TTL                      10330
%token KW_SO_BROADCAST                10331
%token KW_IP_TOS                      10332
%token KW_SO_SNDBUF                   10333
%token KW_SO_RCVBUF                   10334
%token KW_SO_KEEPALIVE                10335
%token KW_SPOOF_SOURCE                10336

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

/* obsolete, compatibility and not-yet supported options */
%token KW_GC_IDLE_THRESHOLD           10390
%token KW_GC_BUSY_THRESHOLD           10391

%token KW_COMPRESS                    10400
%token KW_MAC                         10401
%token KW_AUTH                        10402
%token KW_ENCRYPT                     10403

%token KW_IFDEF                       10410
%token KW_ENDIF                       10411

%token LL_DOTDOT                      10420

%token <cptr> LL_IDENTIFIER           10421
%token <num>  LL_NUMBER               10422
%token <fnum> LL_FLOAT                10423
%token <cptr> LL_STRING               10424
%token <token> LL_TOKEN               10425

%left	KW_OR
%left	KW_AND
%left   KW_NOT

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
#include "plugin.h"


#include "affile.h"
#include "afinter.h"
#include "afstreams.h"
#include "afuser.h"
#include "afprog.h"
#if ENABLE_SQL
#include "afsql.h"
#endif

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
LogParser *last_parser;
LogRewrite *last_rewrite;
gchar *last_include_file;
FilterExprNode *last_filter_expr;

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
%type	<ptr> source_affile
%type	<ptr> source_affile_params
%type	<ptr> source_afpipe_params
%type	<ptr> source_afstreams
%type	<ptr> source_afstreams_params
%type   <ptr> source_afprogram
%type   <ptr> source_afprogram_params
%type   <ptr> source_plugin

%type	<ptr> dest_items
%type	<ptr> dest_item
%type   <ptr> dest_affile
%type	<ptr> dest_affile_params
%type   <ptr> dest_afpipe
%type   <ptr> dest_afpipe_params
%type   <ptr> dest_afuser
%type   <ptr> dest_afprogram
%type   <ptr> dest_afprogram_params
%type   <ptr> dest_afsql
%type   <ptr> dest_afsql_params
%type   <num> dest_afsql_flags
%type   <ptr> dest_plugin

%type	<ptr> log_items
%type	<ptr> log_item

%type   <ptr> log_forks
%type   <ptr> log_fork

%type	<num> log_flags
%type   <num> log_flags_items

%type	<ptr> options_items
%type	<ptr> options_item

%type	<ptr> parser_expr
%type   <num> parser_csv_flags

%type   <ptr> rewrite_expr
%type   <ptr> rewrite_expr_list
%type   <ptr> rewrite_expr_list_build



/* START_DECLS */

%type	<num> yesno
%type   <num> dnsmode
%type   <num> regexp_option_flags
%type	<num> source_reader_option_flags
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
        : stmt ';'
          {
            if (last_include_file && !cfg_lexer_process_include(lexer, last_include_file))
              {
                free(last_include_file);
                last_include_file = NULL;
                YYERROR;
              }
            if (last_include_file)
              {
                free(last_include_file);
                last_include_file = NULL;
              }
          }
          stmts
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
	| KW_INCLUDE include_stmt		{  }
	;

source_stmt
	: string '{' source_items '}'		{ $$ = log_source_group_new($1, $3); free($1); }
	;

filter_stmt
	: string '{'
	  {
	    last_filter_expr = NULL;
	    cfg_lexer_push_context(lexer, LL_CONTEXT_FILTER, "filter expression");
	    if (!cfg_parser_parse(&filter_expr_parser, lexer, (gpointer *) &last_filter_expr))
              {
                cfg_lexer_pop_context(lexer);
                YYERROR;
              }
            cfg_lexer_pop_context(lexer);
	  }
	  '}'                               { $$ = log_filter_rule_new($1, last_filter_expr); free($1); }
	;
	
parser_stmt
        : string '{' parser_expr ';' '}'	{ $$ = log_parser_rule_new($1, $3); free($1); }

rewrite_stmt
        : string '{' rewrite_expr_list '}'	{ $$ = log_rewrite_rule_new($1, $3); free($1); }

dest_stmt
        : string '{' dest_items '}'		{ $$ = log_dest_group_new($1, $3); free($1); }
	;

log_stmt
        : '{' log_items log_forks log_flags '}'		{ LogPipeItem *pi = log_pipe_item_append_tail($2, $3); $$ = log_connection_new(pi, $4); }
	;
	
include_stmt
        : string                                { last_include_file = $1; }

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
	: KW_TEMPLATE '(' string ')'		{ last_template->template = g_strdup($3); free($3); if (!cfg_check_template(last_template)) { YYERROR; } }
	| KW_TEMPLATE_ESCAPE '(' yesno ')'	{ log_template_set_escape(last_template, $3); }
	;


source_items
        : source_item ';' source_items		{ if ($1) {log_drv_append($1, $3); log_drv_unref($3); $$ = $1; } else { YYERROR; } }
	|					{ $$ = NULL; }
	;

source_item
  	: source_afinter			{ $$ = $1; }
	| source_affile				{ $$ = $1; }
	| source_afstreams			{ $$ = $1; }
        | source_afprogram                      { $$ = $1; }
        | source_plugin                         { $$ = $1; }
	;

source_plugin
        : LL_IDENTIFIER
          {
            gchar errbuf[256];

            g_snprintf(errbuf, sizeof(errbuf), "plugin %s", $1);
            cfg_lexer_push_context(lexer, LL_CONTEXT_SOURCE, errbuf);
            last_driver = (LogDriver *) plugin_new_instance(lexer, LL_CONTEXT_SOURCE, $1);
            cfg_lexer_pop_context(lexer);
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

source_affile
	: KW_FILE '(' source_affile_params ')'	{ $$ = $3; }
	| KW_PIPE '(' source_afpipe_params ')'	{ $$ = $3; }
	;

source_affile_params
	: string
	  {
	    last_driver = affile_sd_new($1, 0);
	    free($1);
	    last_reader_options = &((AFFileSourceDriver *) last_driver)->reader_options;
	  }
          source_reader_options                 { $$ = last_driver; }
       ;

source_afpipe_params
	: string
	  {
	    last_driver = affile_sd_new($1, AFFILE_PIPE);
	    free($1);
	    last_reader_options = &((AFFileSourceDriver *) last_driver)->reader_options;
	  }
	  source_afpipe_options				{ $$ = last_driver; }
	;

source_afpipe_options
	: KW_OPTIONAL '(' yesno ')'			{ last_driver->optional = $3; }
	| source_reader_options				{}
	;

source_afprogram
	: KW_PROGRAM '(' source_afprogram_params ')' { $$ = $3; }
	;

source_afprogram_params
	: string
	  {
	    last_driver = afprogram_sd_new($1);
	    free($1);
	    last_reader_options = &((AFProgramSourceDriver *) last_driver)->reader_options;
	  }
	  source_reader_options			{ $$ = last_driver; }
	;
	
source_afstreams
	: KW_IFDEF {
#if ENABLE_SUN_STREAMS
}
        | KW_SUN_STREAMS '(' source_afstreams_params ')'	{ $$ = $3; }
        | KW_ENDIF {
#endif
}
	;
	
source_afstreams_params
	: string
	  {
#if ENABLE_SUN_STREAMS
	    last_driver = afstreams_sd_new($1);
	    free($1);
#endif
	  }
	  source_afstreams_options		{ $$ = last_driver; }
	;
	
source_afstreams_options
	: source_afstreams_option source_afstreams_options
	|
	;

source_afstreams_option
	: KW_IFDEF {
#if ENABLE_SUN_STREAMS
}
	| KW_DOOR '(' string ')'		{ afstreams_sd_set_sundoor(last_driver, $3); free($3); }
	| KW_ENDIF {
#endif
}
	;
	


dest_items
	: dest_item ';' dest_items		{ log_drv_append($1, $3); log_drv_unref($3); $$ = $1; }
	|					{ $$ = NULL; }
	;

dest_item
	: dest_affile				{ $$ = $1; }
	| dest_afpipe				{ $$ = $1; }
	| dest_afuser				{ $$ = $1; }
	| dest_afprogram			{ $$ = $1; }
	| dest_afsql				{ $$ = $1; }
        | dest_plugin                           { $$ = $1; }
	;

dest_plugin
        : LL_IDENTIFIER
          {
            gchar errbuf[256];

            g_snprintf(errbuf, sizeof(errbuf), "plugin %s", $1);
            cfg_lexer_push_context(lexer, LL_CONTEXT_DESTINATION, errbuf);
            last_driver = (LogDriver *) plugin_new_instance(lexer, LL_CONTEXT_DESTINATION, $1);
            cfg_lexer_pop_context(lexer);
            free($1);
            if (!last_driver)
              {
                YYERROR;
              }
            $$ = last_driver;
          }
        ;

dest_affile
	: KW_FILE '(' dest_affile_params ')'	{ $$ = $3; }
	;

dest_affile_params
	: string
	  {
	    last_driver = affile_dd_new($1, 0);
	    free($1);
	    last_writer_options = &((AFFileDestDriver *) last_driver)->writer_options;
	  }
	  dest_affile_options
	  					{ $$ = last_driver; }
	;

dest_affile_options
	: dest_affile_option dest_affile_options		
        |
	;	

dest_affile_option
	: dest_writer_option
	| KW_OPTIONAL '(' yesno ')'		{ last_driver->optional = $3; }
	| KW_OWNER '(' string_or_number ')'	{ affile_dd_set_file_uid(last_driver, $3); free($3); }
	| KW_GROUP '(' string_or_number ')'	{ affile_dd_set_file_gid(last_driver, $3); free($3); }
	| KW_PERM '(' LL_NUMBER ')'		{ affile_dd_set_file_perm(last_driver, $3); }
	| KW_DIR_OWNER '(' string_or_number ')'	{ affile_dd_set_dir_uid(last_driver, $3); free($3); }
	| KW_DIR_GROUP '(' string_or_number ')'	{ affile_dd_set_dir_gid(last_driver, $3); free($3); }
	| KW_DIR_PERM '(' LL_NUMBER ')'		{ affile_dd_set_dir_perm(last_driver, $3); }
	| KW_CREATE_DIRS '(' yesno ')'		{ affile_dd_set_create_dirs(last_driver, $3); }
	| KW_OVERWRITE_IF_OLDER '(' LL_NUMBER ')'	{ affile_dd_set_overwrite_if_older(last_driver, $3); }
	| KW_FSYNC '(' yesno ')'		{ affile_dd_set_fsync(last_driver, $3); }
	| KW_LOCAL_TIME_ZONE '(' string ')'     { affile_dd_set_local_time_zone(last_driver, $3); free($3); }
	;

dest_afpipe
	: KW_PIPE '(' dest_afpipe_params ')'    { $$ = $3; }
	;

dest_afpipe_params
	: string
	  {
	    last_driver = affile_dd_new($1, AFFILE_PIPE);
	    free($1);
	    last_writer_options = &((AFFileDestDriver *) last_driver)->writer_options;
	    last_writer_options->flush_lines = 0;
	  }
	  dest_afpipe_options                   { $$ = last_driver; }
	;

dest_afpipe_options
	: dest_afpipe_option dest_afpipe_options
	|
	;

dest_afpipe_option
	: dest_writer_option
	| KW_OWNER '(' string_or_number ')'	{ affile_dd_set_file_uid(last_driver, $3); free($3); }
	| KW_GROUP '(' string_or_number ')'	{ affile_dd_set_file_gid(last_driver, $3); free($3); }
	| KW_PERM '(' LL_NUMBER ')'		{ affile_dd_set_file_perm(last_driver, $3); }
	;

dest_afuser
	: KW_USERTTY '(' string ')'		{ $$ = afuser_dd_new($3); free($3); }
	;

dest_afprogram
	: KW_PROGRAM '(' dest_afprogram_params ')' { $$ = $3; }
	;

dest_afprogram_params
	: string
	  {
	    last_driver = afprogram_dd_new($1);
	    free($1);
	    last_writer_options = &((AFProgramDestDriver *) last_driver)->writer_options;
	  }
	  dest_writer_options			{ $$ = last_driver; }
	;
	
dest_afsql
        : KW_SQL '(' dest_afsql_params ')'	{ $$ = $3; }
        ;

dest_afsql_params
        :
          {
            #if ENABLE_SQL	
            last_driver = afsql_dd_new();
            #endif /* ENABLE_SQL */
          }
          dest_afsql_options			{ $$ = last_driver; }
        ;

dest_afsql_options
        : dest_afsql_option dest_afsql_options
        |
        ;

dest_afsql_option
        : KW_IFDEF {
#if ENABLE_SQL
}
        | KW_TYPE '(' string ')'		{ afsql_dd_set_type(last_driver, $3); free($3); }
        | KW_HOST '(' string ')'		{ afsql_dd_set_host(last_driver, $3); free($3); }
        | KW_PORT '(' string_or_number ')'	{ afsql_dd_set_port(last_driver, $3); free($3); }
        | KW_USERNAME '(' string ')'		{ afsql_dd_set_user(last_driver, $3); free($3); }
        | KW_PASSWORD '(' string ')'		{ afsql_dd_set_password(last_driver, $3); free($3); }
        | KW_DATABASE '(' string ')'		{ afsql_dd_set_database(last_driver, $3); free($3); }
        | KW_TABLE '(' string ')'		{ afsql_dd_set_table(last_driver, $3); free($3); }
        | KW_COLUMNS '(' string_list ')'	{ afsql_dd_set_columns(last_driver, $3); }
        | KW_INDEXES '(' string_list ')'        { afsql_dd_set_indexes(last_driver, $3); }
        | KW_VALUES '(' string_list ')'		{ afsql_dd_set_values(last_driver, $3); }
	| KW_LOG_FIFO_SIZE '(' LL_NUMBER ')'	{ afsql_dd_set_mem_fifo_size(last_driver, $3); }
	| KW_LOG_DISK_FIFO_SIZE '(' LL_NUMBER ')'	{ afsql_dd_set_disk_fifo_size(last_driver, $3); }
        | KW_FRAC_DIGITS '(' LL_NUMBER ')'         { afsql_dd_set_frac_digits(last_driver, $3); }
	| KW_TIME_ZONE '(' string ')'           { afsql_dd_set_send_time_zone(last_driver,$3); free($3); }
	| KW_LOCAL_TIME_ZONE '(' string ')'     { afsql_dd_set_local_time_zone(last_driver,$3); free($3); }
        | KW_NULL '(' string ')'                { afsql_dd_set_null_value(last_driver, $3); free($3); }
        | KW_FLUSH_LINES '(' LL_NUMBER ')'      { afsql_dd_set_flush_lines(last_driver, $3); }
        | KW_FLUSH_TIMEOUT '(' LL_NUMBER ')'    { afsql_dd_set_flush_timeout(last_driver, $3); }
        | KW_SESSION_STATEMENTS '(' string_list ')' { afsql_dd_set_session_statements(last_driver, $3); }
        | KW_FLAGS '(' dest_afsql_flags ')'     { afsql_dd_set_flags(last_driver, $3); }
        | KW_ENDIF {
#endif /* ENABLE_SQL */
}
        ;

dest_afsql_flags
        : string dest_afsql_flags               { $$ = afsql_dd_lookup_flag($1) | $2; free($1); }
        |                                       { $$ = 0; }
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
	| KW_GC_BUSY_THRESHOLD '(' LL_NUMBER ')' 	{ /* ignored */; }
	| KW_GC_IDLE_THRESHOLD '(' LL_NUMBER ')'	{ /* ignored */; }
	| KW_CREATE_DIRS '(' yesno ')'		{ configuration->create_dirs = $3; }
	| KW_OWNER '(' string_or_number ')'	{ cfg_file_owner_set(configuration, $3); free($3); }
	| KW_GROUP '(' string_or_number ')'	{ cfg_file_group_set(configuration, $3); free($3); }
	| KW_PERM '(' LL_NUMBER ')'		{ cfg_file_perm_set(configuration, $3); }
	| KW_DIR_OWNER '(' string_or_number ')'	{ cfg_dir_owner_set(configuration, $3); free($3); }
	| KW_DIR_GROUP '(' string_or_number ')'	{ cfg_dir_group_set(configuration, $3); free($3); }
	| KW_DIR_PERM '(' LL_NUMBER ')'		{ cfg_dir_perm_set(configuration, $3); }
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



parser_expr
        : KW_CSV_PARSER '('
          {
            last_parser = (LogParser *) log_csv_parser_new();
          }
          parser_csv_opts
          ')'					{ $$ = last_parser; }
        | KW_DB_PARSER '('
          {
            last_parser = (LogParser *) log_db_parser_new();
          }
          parser_db_opts
          ')'                                   { $$ = last_parser; }
        ;

parser_db_opts
        : parser_db_opt parser_db_opts
        |
        ;

/* NOTE: we don't support parser_opt as we don't want the user to specify a template */
parser_db_opt
        : KW_FILE '(' string ')'                { log_db_parser_set_db_file(((LogDBParser *) last_parser), $3); free($3); }
        ;

parser_column_opt
        : parser_opt
        | KW_COLUMNS '(' string_list ')'        { log_column_parser_set_columns((LogColumnParser *) last_parser, $3); }
        ;

parser_opt
        : KW_TEMPLATE '(' string ')'            {
                                                  LogTemplate *template = cfg_check_inline_template(configuration, $3);
                                                  if (!cfg_check_template(template))
                                                    {
                                                      YYERROR;
                                                    }
                                                  log_parser_set_template(last_parser, template);
                                                  free($3);
                                                }
        ;


parser_csv_opts
        : parser_csv_opt parser_csv_opts
        |
        ;

parser_csv_opt
        : parser_column_opt
        | KW_FLAGS '(' parser_csv_flags ')'     { log_csv_parser_set_flags((LogColumnParser *) last_parser, $3); }
        | KW_DELIMITERS '(' string ')'          { log_csv_parser_set_delimiters((LogColumnParser *) last_parser, $3); free($3); }
        | KW_QUOTES '(' string ')'              { log_csv_parser_set_quotes((LogColumnParser *) last_parser, $3); free($3); }
        | KW_QUOTE_PAIRS '(' string ')'         { log_csv_parser_set_quote_pairs((LogColumnParser *) last_parser, $3); free($3); }
        | KW_NULL '(' string ')'                { log_csv_parser_set_null_value((LogColumnParser *) last_parser, $3); free($3); }
        ;

parser_csv_flags
        : string parser_csv_flags               { $$ = log_csv_parser_lookup_flag($1) | $2; free($1); }
        |					{ $$ = 0; }
        ;

rewrite_expr_list
        : rewrite_expr_list_build               { $$ = g_list_reverse($1); }
        ;

rewrite_expr_list_build
        : rewrite_expr rewrite_expr_list_build  { $$ = g_list_append($2, $1); }
        |                                       { $$ = NULL; }
        ;

rewrite_expr
        : KW_SUBST '(' string string
          {
            last_rewrite = log_rewrite_subst_new($4);
            free($4);
          }
          rewrite_expr_opts ')' ';'
          {
            if(!log_rewrite_set_regexp(last_rewrite, $3))
              YYERROR;
            free($3);
            $$ = last_rewrite;
          }
        | KW_SET '(' string
          {
            last_rewrite = log_rewrite_set_new($3);
            free($3);
          }
          rewrite_expr_opts ')' ';'             { $$ = last_rewrite; }
        ;

rewrite_expr_opts
        : rewrite_expr_opt rewrite_expr_opts
        |
        ;

rewrite_expr_opt
        : KW_VALUE '(' string ')'
          {
            const gchar *p = $3;
            if (p[0] == '$')
              {
                msg_warning("Value references in rewrite rules should not use the '$' prefix, those are only needed in templates",
                            evt_tag_str("value", $3),
                            NULL);
                p++;
              }
            last_rewrite->value_handle = log_msg_get_value_handle(p);
            if (log_msg_is_handle_macro(last_rewrite->value_handle))
              {
                msg_warning("Macros are read-only, they cannot be changed in rewrite rules, falling back to MESSAGE instead",
                            evt_tag_str("macro", p),
                            NULL);
                last_rewrite->value_handle = LM_V_MESSAGE;
              }
            free($3);
          }
        | KW_TYPE '(' string ')'
          {
            if (strcmp($3, "glob") == 0)
              {
                msg_error("Rewrite rules do not support glob expressions",
                          NULL);
                YYERROR;
              }
            log_rewrite_set_matcher(last_rewrite, log_matcher_new($3));
            free($3);
          }
        | KW_FLAGS '(' regexp_option_flags ')' { log_rewrite_set_flags(last_rewrite, $3); }
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
        | LL_NUMBER                                { char buf[32]; snprintf(buf, sizeof(buf), "%" G_GINT64_FORMAT, $1); $$ = strdup(buf); }
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
	    if (n == -1)
	      {
	        msg_error("Unknown priority level",
                          evt_tag_str("priority", $1),
                          NULL);
	        YYERROR;
	      }
	    free($1);
            $$ = n;
	  }
        ;

facility_string
        : string
          {
            /* return the numeric value of facility */
	    int n = syslog_name_lookup_facility_by_name($1);
	    if (n == -1)
	      {
	        msg_error("Unknown facility",
	                  evt_tag_str("facility", $1),
	                  NULL);
                YYERROR;
	      }
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
	| KW_TIME_ZONE '(' string ')'		{ last_reader_options->recv_time_zone = g_strdup($3); free($3); }
	| KW_CHECK_HOSTNAME '(' yesno ')'	{ last_reader_options->check_hostname = $3; }
	| KW_FLAGS '(' source_reader_option_flags ')' { last_reader_options->options = $3; }
	| KW_LOG_MSG_SIZE '(' LL_NUMBER ')'	{ last_reader_options->msg_size = $3; }
	| KW_LOG_FETCH_LIMIT '(' LL_NUMBER ')'	{ last_reader_options->fetch_limit = $3; }
	| KW_PAD_SIZE '(' LL_NUMBER ')'		{ last_reader_options->padding = $3; }
	| KW_FOLLOW_FREQ '(' LL_FLOAT ')'		{ last_reader_options->follow_freq = (long) ($3 * 1000); }
	| KW_FOLLOW_FREQ '(' LL_NUMBER ')'		{ last_reader_options->follow_freq = ($3 * 1000); }
	| KW_KEEP_TIMESTAMP '(' yesno ')'	{ last_reader_options->super.keep_timestamp = $3; }
        | KW_ENCODING '(' string ')'		{ last_reader_options->text_encoding = g_strdup($3); free($3); }
        | KW_TAGS '(' string_list ')'           { log_reader_options_set_tags(last_reader_options, $3); }
	| KW_DEFAULT_LEVEL '(' level_string ')'
	  {
	    if (last_reader_options->default_pri == 0xFFFF)
	      last_reader_options->default_pri = LOG_USER;
	    last_reader_options->default_pri = (last_reader_options->default_pri & ~7) | $3;
          }
	| KW_DEFAULT_FACILITY '(' facility_string ')'
	  {
	    if (last_reader_options->default_pri == 0xFFFF)
	      last_reader_options->default_pri = LOG_NOTICE;
	    last_reader_options->default_pri = (last_reader_options->default_pri & 7) | $3;
          }
	;

source_reader_option_flags
	: string source_reader_option_flags     { $$ = log_reader_options_lookup_flag($1) | $2; free($1); }
	|					{ $$ = 0; }
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
	                                          last_writer_options->template = cfg_check_inline_template(configuration, $3);
                                                  if (!cfg_check_template(last_writer_options->template))
	                                            {
	                                              YYERROR;
	                                            }
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

