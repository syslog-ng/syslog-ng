/*
 * Copyright (c) 2002-2014 Balabit
 * Copyright (c) 1998-2013 Balázs Scheidler
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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

%code requires {

/* this block is inserted into cfg-grammar.h, so it is included
   practically all of the syslog-ng code. Please add headers here
   with care. If you need additional headers, please look for a
   massive list of includes further below. */

#pragma GCC diagnostic ignored "-Wswitch-default"
/* YYSTYPE and YYLTYPE is defined by the lexer */
#include "cfg-lexer.h"
#include "cfg-path.h"
#include "afinter.h"
#include "type-hinting.h"
#include "filter/filter-expr-parser.h"
#include "filter/filter-pipe.h"
#include "parser/parser-expr-parser.h"
#include "rewrite/rewrite-expr-parser.h"
#include "logmatcher.h"
#include "logthrdest/logthrdestdrv.h"
#include "logthrsource/logthrsourcedrv.h"
#include "logthrsource/logthrfetcherdrv.h"
#include "str-utils.h"
#include <sys/stat.h>

/* uses struct declarations instead of the typedefs to avoid having to
 * include logreader/logwriter/driver.h, which defines the typedefs.  This
 * is to avoid including unnecessary dependencies into grammars that are not
 * themselves reader/writer based */

extern struct _LogSourceOptions *last_source_options;
extern struct _LogReaderOptions *last_reader_options;
extern struct _LogProtoServerOptions *last_proto_server_options;
extern struct _LogProtoClientOptions *last_proto_client_options;
extern struct _LogWriterOptions *last_writer_options;
extern struct _FilePermOptions *last_file_perm_options;
extern struct _MsgFormatOptions *last_msg_format_options;
extern struct _LogDriver *last_driver;
extern struct _LogParser *last_parser;
extern struct _LogTemplateOptions *last_template_options;
extern struct _LogTemplate *last_template;
extern struct _ValuePairs *last_value_pairs;
extern struct _ValuePairsTransformSet *last_vp_transset;
extern struct _LogMatcherOptions *last_matcher_options;
extern struct _HostResolveOptions *last_host_resolve_options;
extern struct _StatsOptions *last_stats_options;
extern struct _LogRewrite *last_rewrite;

}

%name-prefix "main_"
%lex-param {CfgLexer *lexer}
%parse-param {CfgLexer *lexer}
%parse-param {gpointer *dummy}
%parse-param {gpointer arg}

/* START_DECLS */

%require "2.4.1"
%locations
%define api.pure
%pure-parser
%error-verbose

%code {

# define YYLLOC_DEFAULT(Current, Rhs, N)                                \
  do {                                                                  \
    if (N)                                                              \
      {                                                                 \
        (Current).level = YYRHSLOC(Rhs, 1).level;                       \
        (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;          \
        (Current).first_column = YYRHSLOC (Rhs, 1).first_column;        \
        (Current).last_line    = YYRHSLOC (Rhs, N).last_line;           \
        (Current).last_column  = YYRHSLOC (Rhs, N).last_column;         \
      }                                                                 \
    else                                                                \
      {                                                                 \
        (Current).level = YYRHSLOC(Rhs, 0).level;                       \
        (Current).first_line   = (Current).last_line   =                \
          YYRHSLOC (Rhs, 0).last_line;                                  \
        (Current).first_column = (Current).last_column =                \
          YYRHSLOC (Rhs, 0).last_column;                                \
      }                                                                 \
  } while (0)

#define CHECK_ERROR_WITHOUT_MESSAGE(val, token) do {                    \
    if (!(val))                                                         \
      {                                                                 \
        YYERROR;                                                        \
      }                                                                 \
  } while (0)

#define CHECK_ERROR(val, token, errorfmt, ...) do {                     \
    if (!(val))                                                         \
      {                                                                 \
        if (errorfmt)                                                   \
          {                                                             \
            gchar __buf[256];                                           \
            g_snprintf(__buf, sizeof(__buf), errorfmt, ## __VA_ARGS__); \
            yyerror(& (token), lexer, NULL, NULL, __buf);               \
          }                                                             \
        YYERROR;                                                        \
      }                                                                 \
  } while (0)

#define CHECK_ERROR_GERROR(val, token, error, errorfmt, ...) do {       \
    if (!(val))                                                         \
      {                                                                 \
        if (errorfmt)                                                   \
          {                                                             \
            gchar __buf[256];                                           \
            g_snprintf(__buf, sizeof(__buf), errorfmt ", error=%s", ## __VA_ARGS__, error->message); \
            yyerror(& (token), lexer, NULL, NULL, __buf);               \
          }                                                             \
        g_clear_error(&error);						\
        YYERROR;                                                        \
      }                                                                 \
  } while (0)

#define YYMAXDEPTH 20000


}

/* plugin types, must be equal to the numerical values of the plugin type in plugin.h */

%token LL_CONTEXT_ROOT                1
%token LL_CONTEXT_DESTINATION         2
%token LL_CONTEXT_SOURCE              3
%token LL_CONTEXT_PARSER              4
%token LL_CONTEXT_REWRITE             5
%token LL_CONTEXT_FILTER              6
%token LL_CONTEXT_LOG                 7
%token LL_CONTEXT_BLOCK_DEF           8
%token LL_CONTEXT_BLOCK_REF           9
%token LL_CONTEXT_BLOCK_CONTENT       10
%token LL_CONTEXT_BLOCK_ARG           11
%token LL_CONTEXT_PRAGMA              12
%token LL_CONTEXT_FORMAT              13
%token LL_CONTEXT_TEMPLATE_FUNC       14
%token LL_CONTEXT_INNER_DEST          15
%token LL_CONTEXT_INNER_SRC           16
%token LL_CONTEXT_CLIENT_PROTO        17
%token LL_CONTEXT_SERVER_PROTO        18
%token LL_CONTEXT_OPTIONS             19
%token LL_CONTEXT_HTTP_AUTH_HEADER    20


/* statements */
%token KW_SOURCE                      10000
%token KW_FILTER                      10001
%token KW_PARSER                      10002
%token KW_DESTINATION                 10003
%token KW_LOG                         10004
%token KW_OPTIONS                     10005
%token KW_INCLUDE                     10006
%token KW_BLOCK                       10007
%token KW_JUNCTION                    10008
%token KW_CHANNEL                     10009
%token KW_IF                          10010
%token KW_ELSE                        10011
%token KW_ELIF                        10012

/* source & destination items */
%token KW_INTERNAL                    10020
%token KW_SYSLOG                      10060

/* option items */
%token KW_MARK_FREQ                   10071
%token KW_STATS_FREQ                  10072
%token KW_STATS_LEVEL                 10073
%token KW_STATS_LIFETIME              10074
%token KW_FLUSH_LINES                 10075
%token KW_SUPPRESS                    10076
%token KW_FLUSH_TIMEOUT               10077
%token KW_LOG_MSG_SIZE                10078
%token KW_FILE_TEMPLATE               10079
%token KW_PROTO_TEMPLATE              10080
%token KW_MARK_MODE                   10081
%token KW_ENCODING                    10082
%token KW_TYPE                        10083
%token KW_STATS_MAX_DYNAMIC           10084
%token KW_MIN_IW_SIZE_PER_READER      10085
%token KW_BATCH_LINES                 10087
%token KW_BATCH_TIMEOUT               10088
%token KW_TRIM_LARGE_MESSAGES         10089

%token KW_CHAIN_HOSTNAMES             10090
%token KW_NORMALIZE_HOSTNAMES         10091
%token KW_KEEP_HOSTNAME               10092
%token KW_CHECK_HOSTNAME              10093
%token KW_BAD_HOSTNAME                10094

%token KW_KEEP_TIMESTAMP              10100

%token KW_USE_DNS                     10110
%token KW_USE_FQDN                    10111
%token KW_CUSTOM_DOMAIN               10112

%token KW_DNS_CACHE                   10120
%token KW_DNS_CACHE_SIZE              10121

%token KW_DNS_CACHE_EXPIRE            10130
%token KW_DNS_CACHE_EXPIRE_FAILED     10131
%token KW_DNS_CACHE_HOSTS             10132

%token KW_PERSIST_ONLY                10140
%token KW_USE_RCPTID                  10141
%token KW_USE_UNIQID                  10142

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
%token KW_THREADED                    10171
%token KW_PASS_UNIX_CREDENTIALS       10231

%token KW_PERSIST_NAME                10302

%token KW_READ_OLD_RECORDS            10304

/* log statement options */
%token KW_FLAGS                       10190

/* reader options */
%token KW_PAD_SIZE                    10200
%token KW_TIME_ZONE                   10201
%token KW_RECV_TIME_ZONE              10202
%token KW_SEND_TIME_ZONE              10203
%token KW_LOCAL_TIME_ZONE             10204
%token KW_FORMAT                      10205

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
%token KW_TEMPLATE_FUNCTION           10272

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
%token KW_NETMASK6                    10357

/* parser items */

/* rewrite items */
%token KW_REWRITE                     10370
%token KW_CONDITION                   10371
%token KW_VALUE                       10372

/* yes/no switches */

%token KW_YES                         10380
%token KW_NO                          10381

%token KW_IFDEF                       10410
%token KW_ENDIF                       10411

%token LL_DOTDOT                      10420
%token LL_DOTDOTDOT                   10421
%token LL_PRAGMA                      10422
%token LL_EOL                         10423
%token LL_ERROR                       10424
%token LL_ARROW                       10425

%token <cptr> LL_IDENTIFIER           10430
%token <num>  LL_NUMBER               10431
%token <fnum> LL_FLOAT                10432
%token <cptr> LL_STRING               10433
%token <token> LL_TOKEN               10434
%token <cptr> LL_BLOCK                10435

%destructor { free($$); } <cptr>

/* value pairs */
%token KW_VALUE_PAIRS                 10500
%token KW_EXCLUDE                     10502
%token KW_PAIR                        10503
%token KW_KEY                         10504
%token KW_SCOPE                       10505
%token KW_SHIFT                       10506
%token KW_SHIFT_LEVELS                10507
%token KW_REKEY                       10508
%token KW_ADD_PREFIX                  10509
%token KW_REPLACE_PREFIX              10510

%token KW_ON_ERROR                    10511

%token KW_RETRIES                     10512

%token KW_FETCH_NO_DATA_DELAY         10513
/* END_DECLS */

%code {

#include "cfg-parser.h"
#include "cfg.h"
#include "cfg-tree.h"
#include "cfg-block.h"
#include "template/templates.h"
#include "template/user-function.h"
#include "logreader.h"
#include "logpipe.h"
#include "parser/parser-expr.h"
#include "rewrite/rewrite-expr.h"
#include "rewrite/rewrite-expr-parser.h"
#include "filter/filter-expr-parser.h"
#include "value-pairs/value-pairs.h"
#include "file-perms.h"
#include "block-ref-parser.h"
#include "plugin.h"
#include "logwriter.h"
#include "messages.h"

#include "syslog-names.h"

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cfg-grammar.h"

LogDriver *last_driver;
LogParser *last_parser;
LogSourceOptions *last_source_options;
LogProtoServerOptions *last_proto_server_options;
LogProtoClientOptions *last_proto_client_options;
LogReaderOptions *last_reader_options;
LogWriterOptions *last_writer_options;
MsgFormatOptions *last_msg_format_options;
FilePermOptions *last_file_perm_options;
LogTemplateOptions *last_template_options;
LogTemplate *last_template;
CfgArgs *last_block_args;
ValuePairs *last_value_pairs;
ValuePairsTransformSet *last_vp_transset;
LogMatcherOptions *last_matcher_options;
HostResolveOptions *last_host_resolve_options;
StatsOptions *last_stats_options;
DNSCacheOptions *last_dns_cache_options;
LogRewrite *last_rewrite;

}

%type   <ptr> expr_stmt
%type   <ptr> source_stmt
%type   <ptr> dest_stmt
%type   <ptr> filter_stmt
%type   <ptr> parser_stmt
%type   <ptr> rewrite_stmt
%type   <ptr> log_stmt
%type   <ptr> plugin_stmt

/* START_DECLS */

%type   <ptr> source_content
%type	<ptr> source_items
%type	<ptr> source_item
%type   <ptr> source_afinter
%type   <ptr> source_plugin
%type   <ptr> source_afinter_params

%type   <ptr> dest_content
%type	<ptr> dest_items
%type	<ptr> dest_item
%type   <ptr> dest_plugin

%type   <ptr> template_content
%type   <ptr> template_content_list

%type   <ptr> filter_content

%type   <ptr> parser_content

%type   <ptr> rewrite_content

%type	<ptr> log_items
%type	<ptr> log_item

%type   <ptr> log_last_junction
%type   <ptr> log_junction
%type   <ptr> log_content
%type   <ptr> log_conditional
%type   <ptr> log_if
%type   <ptr> log_forks
%type   <ptr> log_fork

%type	<num> log_flags
%type   <num> log_flags_items

%type   <ptr> value_pair_option

%type	<num> yesno
%type   <num> dnsmode
%type	<num> dest_writer_options_flags

%type	<cptr> string
%type	<cptr> string_or_number
%type	<cptr> normalized_flag
%type   <ptr> string_list
%type   <ptr> string_list_build
%type   <num> facility_string
%type   <num> level_string

%type   <num> positive_integer
%type   <num> positive_integer64
%type   <num> nonnegative_integer
%type   <num> nonnegative_integer64
%type	<cptr> path_no_check
%type	<cptr> path_secret
%type	<cptr> path_check
%type	<cptr> path

/* END_DECLS */

%type   <ptr> template_def
%type   <ptr> template_block
%type   <ptr> template_simple
%type   <ptr> template_fn


%%

start
        : stmts
	;

stmts
        : stmt semicolons stmts
	|
	;

stmt
        : expr_stmt
          {
            CHECK_ERROR(cfg_tree_add_object(&configuration->tree, $1) || cfg_allow_config_dups(configuration), @1, "duplicate %s definition", log_expr_node_get_content_name(((LogExprNode *) $1)->content));
          }
	| template_stmt
	| options_stmt
	| block_stmt
	| plugin_stmt
	;

expr_stmt
        : source_stmt
	| dest_stmt
	| filter_stmt
	| parser_stmt
        | rewrite_stmt
	| log_stmt
        ;

source_stmt
        : KW_SOURCE string '{' source_content '}'
          {
            $$ = log_expr_node_new_source($2, $4, &@1);
            free($2);
          }
	;
dest_stmt
       : KW_DESTINATION string '{' dest_content '}'
          {
            $$ = log_expr_node_new_destination($2, $4, &@1);
            free($2);
          }
	;


filter_stmt
        : KW_FILTER string '{' filter_content '}'
          {
            /* NOTE: the filter() subexpression (e.g. the one that invokes
             * one filter expression from another) depends on the layout
             * parsed into the cfg-tree when looking up the referenced
             * filter expression.  So when changing how a filter statement
             * is transformed into the cfg-tree, make sure you check
             * filter-call.c, especially filter_call_init() function as
             * well.
             */

            $$ = log_expr_node_new_filter($2, $4, &@1);
            free($2);
          }
        ;

parser_stmt
        : KW_PARSER string '{' parser_content '}'
          {
            $$ = log_expr_node_new_parser($2, $4, &@1);
            free($2);
          }
        ;

rewrite_stmt
        : KW_REWRITE string '{' rewrite_content '}'
          {
            $$ = log_expr_node_new_rewrite($2, $4, &@1);
            free($2);
          }

log_stmt
        : KW_LOG
          { cfg_lexer_push_context(lexer, LL_CONTEXT_LOG, NULL, "log statement"); }
          '{' log_content '}'
          { cfg_lexer_pop_context(lexer); }
          {
            $$ = $4;
          }

	;


plugin_stmt
        : LL_IDENTIFIER
          {
            Plugin *p;
            gint context = LL_CONTEXT_ROOT;
            gpointer result;

            p = cfg_find_plugin(configuration, context, $1);
            CHECK_ERROR(p, @1, "%s plugin %s not found", cfg_lexer_lookup_context_name_by_type(context), $1);

            result = cfg_parse_plugin(configuration, p, &@1, NULL);
            free($1);
            if (!result)
              YYERROR;
            $$ = NULL;
          }

/* START_RULES */

source_content
        :
          { cfg_lexer_push_context(lexer, LL_CONTEXT_SOURCE, NULL, "source statement"); }
          source_items
          { cfg_lexer_pop_context(lexer); }
          {
            $$ = log_expr_node_new_junction($2, &@$);
          }
        ;

source_items
        : source_item semicolons source_items	{ $$ = log_expr_node_append_tail(log_expr_node_new_pipe($1, &@1), $3); }
        | log_fork semicolons source_items      { $$ = log_expr_node_append_tail($1,  $3); }
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

            p = cfg_find_plugin(configuration, context, $1);
            CHECK_ERROR(p, @1, "%s plugin %s not found", cfg_lexer_lookup_context_name_by_type(context), $1);

            last_driver = (LogDriver *) cfg_parse_plugin(configuration, p, &@1, NULL);
            free($1);
            if (!last_driver)
              {
                YYERROR;
              }
            $$ = last_driver;
          }
        ;

source_afinter
	: KW_INTERNAL '(' source_afinter_params ')'			{ $$ = $3; }
	;

source_afinter_params
        : {
            last_driver = afinter_sd_new(configuration);
            last_source_options = &((AFInterSourceDriver *) last_driver)->source_options;
          }
          source_afinter_options { $$ = last_driver; }
        ;

source_afinter_options
        : source_afinter_option source_afinter_options
        |
        ;

source_afinter_option
        : source_option
        ;


filter_content
        : {
            FilterExprNode *last_filter_expr = NULL;

	    CHECK_ERROR_WITHOUT_MESSAGE(cfg_parser_parse(&filter_expr_parser, lexer, (gpointer *) &last_filter_expr, NULL), @$);

            $$ = log_expr_node_new_pipe(log_filter_pipe_new(last_filter_expr, configuration), &@$);
	  }
	;

parser_content
        :
          {
            LogExprNode *last_parser_expr = NULL;

            CHECK_ERROR_WITHOUT_MESSAGE(cfg_parser_parse(&parser_expr_parser, lexer, (gpointer *) &last_parser_expr, NULL), @$);
            $$ = last_parser_expr;
          }
        ;

rewrite_content
        :
          {
            LogExprNode *last_rewrite_expr = NULL;

            CHECK_ERROR_WITHOUT_MESSAGE(cfg_parser_parse(&rewrite_expr_parser, lexer, (gpointer *) &last_rewrite_expr, NULL), @$);
            $$ = last_rewrite_expr;
          }
        ;

dest_content
         : { cfg_lexer_push_context(lexer, LL_CONTEXT_DESTINATION, NULL, "destination statement"); }
            dest_items
           { cfg_lexer_pop_context(lexer); }
           {
             $$ = log_expr_node_new_junction($2, &@$);
           }
         ;


dest_items
        /* all destination drivers are added as an independent branch in a junction*/
        : dest_item semicolons dest_items	{ $$ = log_expr_node_append_tail(log_expr_node_new_pipe($1, &@1), $3); }
        | log_fork semicolons dest_items        { $$ = log_expr_node_append_tail($1,  $3); }
	|					{ $$ = NULL; }
	;

dest_item
        : dest_plugin                           { $$ = $1; }
	;

dest_plugin
        : LL_IDENTIFIER
          {
            Plugin *p;
            gint context = LL_CONTEXT_DESTINATION;

            p = cfg_find_plugin(configuration, context, $1);
            CHECK_ERROR(p, @1, "%s plugin %s not found", cfg_lexer_lookup_context_name_by_type(context), $1);

            last_driver = (LogDriver *) cfg_parse_plugin(configuration, p, &@1, NULL);
            free($1);
            if (!last_driver)
              {
                YYERROR;
              }
            $$ = last_driver;
          }
        ;

log_items
	: log_item semicolons log_items		{ log_expr_node_append_tail($1, $3); $$ = $1; }
	|					{ $$ = NULL; }
	;

log_item
        : KW_SOURCE '(' string ')'		{ $$ = log_expr_node_new_source_reference($3, &@$); free($3); }
        | KW_SOURCE '{' source_content '}'      { $$ = log_expr_node_new_source(NULL, $3, &@$); }
        | KW_FILTER '(' string ')'		{ $$ = log_expr_node_new_filter_reference($3, &@$); free($3); }
        | KW_FILTER '{' filter_content '}'      { $$ = log_expr_node_new_filter(NULL, $3, &@$); }
        | KW_PARSER '(' string ')'              { $$ = log_expr_node_new_parser_reference($3, &@$); free($3); }
        | KW_PARSER '{' parser_content '}'      { $$ = log_expr_node_new_parser(NULL, $3, &@$); }
        | KW_REWRITE '(' string ')'             { $$ = log_expr_node_new_rewrite_reference($3, &@$); free($3); }
        | KW_REWRITE '{' rewrite_content '}'    { $$ = log_expr_node_new_rewrite(NULL, $3, &@$); }
        | KW_DESTINATION '(' string ')'		{ $$ = log_expr_node_new_destination_reference($3, &@$); free($3); }
        | KW_DESTINATION '{' dest_content '}'   { $$ = log_expr_node_new_destination(NULL, $3, &@$); }
        | log_conditional			{ $$ = $1; }
        | log_junction                          { $$ = $1; }
	;

log_junction
        : KW_JUNCTION '{' log_forks '}'         { $$ = log_expr_node_new_junction($3, &@$); }
        ;

log_last_junction

        /* this rule matches the last set of embedded log {}
         * statements at the end of the log {} block.
         * It is the final junction and was the only form of creating
         * a processing tree before syslog-ng 3.4.
         *
         * We emulate if the user was writing junction {} explicitly.
         */
        : log_forks                             { $$ = $1 ? log_expr_node_new_junction($1, &@1) :  NULL; }
        ;


log_forks
        : log_fork semicolons log_forks		{ log_expr_node_append_tail($1, $3); $$ = $1; }
        |                                       { $$ = NULL; }
        ;

log_fork
        : KW_LOG '{' log_content '}'            { $$ = $3; }
        | KW_CHANNEL '{' log_content '}'        { $$ = $3; }
        ;

log_conditional
        : log_if				{ $$ = $1; }
        | log_if KW_ELSE '{' log_content '}'
          {
            log_expr_node_conditional_set_false_branch_of_the_last_if($1, $4);
            $$ = $1;
          }
        ;

log_if
        : KW_IF '(' filter_content ')' '{' log_content '}'
          {
            $$ = log_expr_node_new_conditional_with_filter($3, $6, &@$);
          }
        | KW_IF '{' log_content '}'
          {
            $$ = log_expr_node_new_conditional_with_block($3, &@$);
          }
        | log_if KW_ELIF '(' filter_content ')' '{' log_content '}'
          {
            LogExprNode *false_branch;

            false_branch = log_expr_node_new_conditional_with_filter($4, $7, &@$);
            log_expr_node_conditional_set_false_branch_of_the_last_if($1, false_branch);
            $$ = $1;
          }
        | log_if KW_ELIF '{' log_content '}'
          {
            LogExprNode *false_branch;

            false_branch = log_expr_node_new_conditional_with_block($4, &@$);
            log_expr_node_conditional_set_false_branch_of_the_last_if($1, false_branch);
            $$ = $1;
          }
        ;

log_content
        : log_items log_last_junction log_flags                { $$ = log_expr_node_new_log(log_expr_node_append_tail($1, $2), $3, &@$); }
        ;

log_flags
	: KW_FLAGS '(' log_flags_items ')' semicolons	{ $$ = $3; }
	|					{ $$ = 0; }
	;

log_flags_items
	: normalized_flag log_flags_items	{ $$ = log_expr_node_lookup_flag($1) | $2; free($1); }
	|					{ $$ = 0; }
	;

/* END_RULES */

options_stmt
        : KW_OPTIONS
          { cfg_lexer_push_context(lexer, LL_CONTEXT_OPTIONS, NULL, "global options"); }
          '{' options_items '}'
          { cfg_lexer_pop_context(lexer); }
	;

template_stmt
        : template_def
          {
            CHECK_ERROR(cfg_tree_add_template(&configuration->tree, $1) || cfg_allow_config_dups(configuration), @1, "duplicate template");
          }
        | template_fn
          {
            user_template_function_register(configuration, last_template->name, last_template);
            log_template_unref(last_template);
            last_template = NULL;
          }
        ;

template_def
        : template_block
        | template_simple
        ;

template_block
	: KW_TEMPLATE string
	  {
	    last_template = log_template_new(configuration, $2);
	  }
	  '{' template_items '}'						{ $$ = last_template; free($2); }
        ;

template_simple
        : KW_TEMPLATE string
          {
	    last_template = log_template_new(configuration, $2);
          }
          template_content_inner						{ $$ = last_template; free($2); }
	;

template_fn
        : KW_TEMPLATE_FUNCTION string
          {
	    last_template = log_template_new(configuration, $2);
          }
          template_content_inner						{ $$ = last_template; free($2); }
	;

template_items
	: template_item semicolons template_items
	|
	;

/* START_RULES */

template_content_inner
        : string
        {
          GError *error = NULL;

          CHECK_ERROR(log_template_compile(last_template, $1, &error), @1, "Error compiling template (%s)", error->message);
          free($1);
        }
        | LL_IDENTIFIER '(' string ')'
        {
          GError *error = NULL;

          CHECK_ERROR(log_template_compile(last_template, $3, &error), @3, "Error compiling template (%s)", error->message);
          free($3);

          CHECK_ERROR(log_template_set_type_hint(last_template, $1, &error), @1, "Error setting the template type-hint (%s)", error->message);
          free($1);
        }
        ;

template_content
        : { last_template = log_template_new(configuration, NULL); } template_content_inner	{ $$ = last_template; }
        ;

template_content_list
	: template_content template_content_list { $$ = g_list_prepend($2, $1); }
	| { $$ = NULL; }
	;

/* END_RULES */

template_item
	: KW_TEMPLATE '(' template_content_inner ')'
	| KW_TEMPLATE_ESCAPE '(' yesno ')'	{ log_template_set_escape(last_template, $3); }
	;


block_stmt
        : KW_BLOCK
          { cfg_lexer_push_context(lexer, LL_CONTEXT_BLOCK_DEF, block_def_keywords, "block definition"); }
          LL_IDENTIFIER LL_IDENTIFIER
          '(' { last_block_args = cfg_args_new(); } block_definition ')'
          { cfg_lexer_push_context(lexer, LL_CONTEXT_BLOCK_CONTENT, NULL, "block content"); }
          LL_BLOCK
          {
            CfgBlockGenerator *block;

            /* block content */
            cfg_lexer_pop_context(lexer);
            /* block definition */
            cfg_lexer_pop_context(lexer);

            gint context_type = cfg_lexer_lookup_context_type_by_name($3);
            CHECK_ERROR(context_type, @3, "unknown context \"%s\"", $3);

            block = cfg_block_new(context_type, $4, $10, last_block_args, &@1);
            cfg_lexer_register_generator_plugin(&configuration->plugin_context, block);
            free($3);
            free($4);
            free($10);
            last_block_args = NULL;
          }
        ;

block_definition
        : block_args
        | block_args LL_DOTDOTDOT			{ cfg_args_accept_varargs(last_block_args); }
        ;

block_args
        : block_arg block_args
        |
        ;

block_arg
        : LL_IDENTIFIER
          {
            cfg_lexer_push_context(lexer, LL_CONTEXT_BLOCK_ARG, NULL, "block argument");
          }
          LL_BLOCK
          {
            cfg_lexer_pop_context(lexer);
            cfg_args_set(last_block_args, $1, $3); free($1); free($3);
          }
        ;

options_items
	: options_item semicolons options_items
	|
	;

options_item
	: KW_MARK_FREQ '(' nonnegative_integer ')'		{ configuration->mark_freq = $3; }
	| KW_FLUSH_LINES '(' nonnegative_integer ')'		{ configuration->flush_lines = $3; }
        | KW_MARK_MODE '(' KW_INTERNAL ')'         { cfg_set_mark_mode(configuration, "internal"); }
        | KW_MARK_MODE '(' string ')'
          {
            CHECK_ERROR(cfg_lookup_mark_mode($3) > 0 && cfg_lookup_mark_mode($3) != MM_GLOBAL, @3, "illegal global mark-mode");
            cfg_set_mark_mode(configuration, $3);
            free($3);
          }
	| KW_FLUSH_TIMEOUT '(' positive_integer ')'     { }
	| KW_CHAIN_HOSTNAMES '(' yesno ')'	{ configuration->chain_hostnames = $3; }
	| KW_KEEP_HOSTNAME '(' yesno ')'	{ configuration->keep_hostname = $3; }
	| KW_CHECK_HOSTNAME '(' yesno ')'	{ configuration->check_hostname = $3; }
	| KW_BAD_HOSTNAME '(' string ')'	{ cfg_bad_hostname_set(configuration, $3); free($3); }
	| KW_TIME_REOPEN '(' positive_integer ')'		{ configuration->time_reopen = $3; }
	| KW_TIME_REAP '(' nonnegative_integer ')'		{ configuration->time_reap = $3; }
	| KW_TIME_SLEEP '(' nonnegative_integer ')'	{}
	| KW_SUPPRESS '(' nonnegative_integer ')'		{ configuration->suppress = $3; }
	| KW_THREADED '(' yesno ')'		{ configuration->threaded = $3; }
	| KW_PASS_UNIX_CREDENTIALS '(' yesno ')' { configuration->pass_unix_credentials = $3; }
	| KW_USE_RCPTID '(' yesno ')'		{ cfg_set_use_uniqid($3); }
	| KW_USE_UNIQID '(' yesno ')'		{ cfg_set_use_uniqid($3); }
	| KW_LOG_FIFO_SIZE '(' positive_integer ')'	{ configuration->log_fifo_size = $3; }
	| KW_LOG_IW_SIZE '(' positive_integer ')'	{ msg_warning("WARNING: Support for the global log-iw-size() option was removed, please use a per-source log-iw-size()", cfg_lexer_format_location_tag(lexer, &@1)); }
	| KW_LOG_FETCH_LIMIT '(' positive_integer ')'	{ msg_warning("WARNING: Support for the global log-fetch-limit() option was removed, please use a per-source log-fetch-limit()", cfg_lexer_format_location_tag(lexer, &@1)); }
	| KW_LOG_MSG_SIZE '(' positive_integer ')'	{ configuration->log_msg_size = $3; }
	| KW_TRIM_LARGE_MESSAGES '(' yesno ')'	{ configuration->trim_large_messages = $3; }
	| KW_KEEP_TIMESTAMP '(' yesno ')'	{ configuration->keep_timestamp = $3; }
	| KW_CREATE_DIRS '(' yesno ')'		{ configuration->create_dirs = $3; }
	| KW_CUSTOM_DOMAIN '(' string ')'	{ configuration->custom_domain = g_strdup($3); free($3); }
	| KW_FILE_TEMPLATE '(' string ')'	{ configuration->file_template_name = g_strdup($3); free($3); }
	| KW_PROTO_TEMPLATE '(' string ')'	{ configuration->proto_template_name = g_strdup($3); free($3); }
	| KW_RECV_TIME_ZONE '(' string ')'	{ configuration->recv_time_zone = g_strdup($3); free($3); }
	| KW_MIN_IW_SIZE_PER_READER '(' positive_integer ')' { configuration->min_iw_size_per_reader = $3; }
	| { last_template_options = &configuration->template_options; } template_option
	| { last_host_resolve_options = &configuration->host_resolve_options; } host_resolve_option
	| { last_stats_options = &configuration->stats_options; } stat_option
	| { last_dns_cache_options = &configuration->dns_cache_options; } dns_cache_option
	| { last_file_perm_options = &configuration->file_perm_options; } file_perm_option
	| LL_IDENTIFIER
          {
            Plugin *p;
            gint context = LL_CONTEXT_OPTIONS;

            p = cfg_find_plugin(configuration, context, $1);
            CHECK_ERROR(p, @1, "%s plugin %s not found", cfg_lexer_lookup_context_name_by_type(context), $1);

            cfg_parse_plugin(configuration, p, &@1, NULL);
            free($1);
          }
	;

stat_option
	: KW_STATS_FREQ '(' nonnegative_integer ')'          { last_stats_options->log_freq = $3; }
	| KW_STATS_LEVEL '(' nonnegative_integer ')'         { last_stats_options->level = $3; }
	| KW_STATS_LIFETIME '(' positive_integer ')'      { last_stats_options->lifetime = $3; }
  | KW_STATS_MAX_DYNAMIC '(' nonnegative_integer ')'   { last_stats_options->max_dynamic = $3; }
	;

dns_cache_option
	: KW_DNS_CACHE_SIZE '(' positive_integer ')'	{ last_dns_cache_options->cache_size = $3; }
	| KW_DNS_CACHE_EXPIRE '(' positive_integer ')'	{ last_dns_cache_options->expire = $3; }
	| KW_DNS_CACHE_EXPIRE_FAILED '(' positive_integer ')'
	                                        { last_dns_cache_options->expire_failed = $3; }
	| KW_DNS_CACHE_HOSTS '(' string ')'     { last_dns_cache_options->hosts = g_strdup($3); free($3); }
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

nonnegative_integer64
        : LL_NUMBER
          {
            CHECK_ERROR(($1 >= 0), @1, "It cannot be negative");
          }
        ;

nonnegative_integer
        : nonnegative_integer64
          {
            CHECK_ERROR(($1 <= G_MAXINT32), @1, "Must be smaller than 2^31");
          }
        ;

positive_integer64
        : LL_NUMBER
          {
            CHECK_ERROR(($1 > 0), @1, "Must be positive");
          }
        ;

positive_integer
        : positive_integer64
          {
            CHECK_ERROR(($1 <= G_MAXINT32), @1, "Must be smaller than 2^31");
          }
        ;

string_or_number
        : string                                { $$ = $1; }
        | LL_NUMBER                             { $$ = strdup(lexer->token_text->str); }
        | LL_FLOAT                              { $$ = strdup(lexer->token_text->str); }
        ;

path
	: string
	  {
            struct stat buffer;
            int ret = stat($1, &buffer);
            CHECK_ERROR((ret == 0), @1, "File \"%s\" not found: %s", $1, strerror(errno));
            $$ = $1;
	  }
	;	

path_check
    : path { cfg_path_track_file(configuration, $1, "path_check"); }
    ;
	
path_secret
    : path { cfg_path_track_file(configuration, $1, "path_secret"); }
    ;
		
path_no_check
    : string { cfg_path_track_file(configuration, $1, "path_no_check"); }
    ;
	
normalized_flag
        : string                                { $$ = normalize_flag($1); free($1); }
        ;

string_list
        : string_list_build                     { $$ = $1; }
        ;

string_list_build
        : string string_list_build		{ $$ = g_list_prepend($2, g_strdup($1)); free($1); }
        |					{ $$ = NULL; }
        ;

semicolons
        : ';'
        | ';' semicolons
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

parser_opt
        : KW_TEMPLATE '(' string ')'            {
                                                  LogTemplate *template;
                                                  GError *error = NULL;

                                                  template = cfg_tree_check_inline_template(&configuration->tree, $3, &error);
                                                  CHECK_ERROR_GERROR(template != NULL, @3, error, "Error compiling template");
                                                  log_parser_set_template(last_parser, template);
                                                  free($3);
                                                }
        ;

driver_option
        : KW_PERSIST_NAME '(' string ')' { log_pipe_set_persist_name(&last_driver->super, $3); free($3); }
        ;

/* All source drivers should incorporate this rule, implies driver_option */
source_driver_option
        : LL_IDENTIFIER
          {
            Plugin *p;
            gint context = LL_CONTEXT_INNER_SRC;
            gpointer value;

            p = cfg_find_plugin(configuration, context, $1);
            CHECK_ERROR(p, @1, "%s plugin %s not found", cfg_lexer_lookup_context_name_by_type(context), $1);

            value = cfg_parse_plugin(configuration, p, &@1, last_driver);

            free($1);
            if (!value)
              {
                YYERROR;
              }
            if (!log_driver_add_plugin(last_driver, (LogDriverPlugin *) value))
              {
                log_driver_plugin_free(value);
                CHECK_ERROR(TRUE, @1, "Error while registering the plugin %s in this destination", $1);
              }
          }
        | driver_option
        ;

/* implies driver_option */
dest_driver_option
        /* NOTE: plugins need to set "last_driver" in order to incorporate this rule in their grammar */

	: KW_LOG_FIFO_SIZE '(' positive_integer ')'	{ ((LogDestDriver *) last_driver)->log_fifo_size = $3; }
	| KW_THROTTLE '(' nonnegative_integer ')'         { ((LogDestDriver *) last_driver)->throttle = $3; }
        | LL_IDENTIFIER
          {
            Plugin *p;
            gint context = LL_CONTEXT_INNER_DEST;
            gpointer value;

            p = cfg_find_plugin(configuration, context, $1);
            CHECK_ERROR(p, @1, "%s plugin %s not found", cfg_lexer_lookup_context_name_by_type(context), $1);

            value = cfg_parse_plugin(configuration, p, &@1, last_driver);

            free($1);
            if (!value)
              {
                YYERROR;
              }
            if (!log_driver_add_plugin(last_driver, (LogDriverPlugin *) value))
              {
                log_driver_plugin_free(value);
                CHECK_ERROR(TRUE, @1, "Error while registering the plugin %s in this destination", $1);
              }
          }
        | driver_option
        ;

/* implies dest_driver_option */
threaded_dest_driver_option
	: KW_RETRIES '(' positive_integer ')'
        {
          log_threaded_dest_driver_set_max_retries_on_error(last_driver, $3);
        }
        | KW_BATCH_LINES '(' nonnegative_integer ')' { log_threaded_dest_driver_set_batch_lines(last_driver, $3); }
        | KW_BATCH_TIMEOUT '(' positive_integer ')' { log_threaded_dest_driver_set_batch_timeout(last_driver, $3); }
        | dest_driver_option
        ;

/* implies source_driver_option and source_option */
threaded_source_driver_option
	: KW_FORMAT '(' string ')' { log_threaded_source_driver_get_parse_options(last_driver)->format = g_strdup($3); free($3); }
        | KW_FLAGS '(' threaded_source_driver_option_flags ')'
        | { last_msg_format_options = log_threaded_source_driver_get_parse_options(last_driver); } msg_format_option
        | { last_source_options = log_threaded_source_driver_get_source_options(last_driver); } source_option
        | source_driver_option
        ;

threaded_fetcher_driver_option
        : KW_FETCH_NO_DATA_DELAY '(' nonnegative_integer ')' { log_threaded_fetcher_driver_set_fetch_no_data_delay(last_driver, $3); }
        ;

threaded_source_driver_option_flags
	: string threaded_source_driver_option_flags
        {
          CHECK_ERROR(msg_format_options_process_flag(log_threaded_source_driver_get_parse_options(last_driver), $1), @1, "Unknown flag %s", $1);
          free($1);
        }
        |
        ;

/* LogSource related options */
source_option
        /* NOTE: plugins need to set "last_source_options" in order to incorporate this rule in their grammar */
	: KW_LOG_IW_SIZE '(' positive_integer ')'	{ last_source_options->init_window_size = $3; }
	| KW_CHAIN_HOSTNAMES '(' yesno ')'	{ last_source_options->chain_hostnames = $3; }
	| KW_KEEP_HOSTNAME '(' yesno ')'	{ last_source_options->keep_hostname = $3; }
	| KW_PROGRAM_OVERRIDE '(' string ')'	{ last_source_options->program_override = g_strdup($3); free($3); }
	| KW_HOST_OVERRIDE '(' string ')'	{ last_source_options->host_override = g_strdup($3); free($3); }
	| KW_LOG_PREFIX '(' string ')'	        { gchar *p = strrchr($3, ':'); if (p) *p = 0; last_source_options->program_override = g_strdup($3); free($3); }
	| KW_KEEP_TIMESTAMP '(' yesno ')'	{ last_source_options->keep_timestamp = $3; }
	| KW_READ_OLD_RECORDS '(' yesno ')'	{ last_source_options->read_old_records = $3; }
        | KW_TAGS '(' string_list ')'		{ log_source_options_set_tags(last_source_options, $3); }
        | { last_host_resolve_options = &last_source_options->host_resolve_options; } host_resolve_option
        ;

/* LogReader related options, implies source_option */
source_reader_option
        /* NOTE: plugins need to set "last_reader_options" in order to incorporate this rule in their grammar */

	: KW_CHECK_HOSTNAME '(' yesno ')'	{ last_reader_options->check_hostname = $3; }
	| KW_FLAGS '(' source_reader_option_flags ')'
	| KW_LOG_FETCH_LIMIT '(' positive_integer ')'	{ last_reader_options->fetch_limit = $3; }
        | KW_FORMAT '(' string ')'              { last_reader_options->parse_options.format = g_strdup($3); free($3); }
        | { last_source_options = &last_reader_options->super; } source_option
        | { last_proto_server_options = &last_reader_options->proto_options.super; } source_proto_option
        | { last_msg_format_options = &last_reader_options->parse_options; } msg_format_option
	;

source_reader_option_flags
        : string source_reader_option_flags     { CHECK_ERROR(log_reader_options_process_flag(last_reader_options, $1), @1, "Unknown flag %s", $1); free($1); }
        | KW_CHECK_HOSTNAME source_reader_option_flags     { log_reader_options_process_flag(last_reader_options, "check-hostname"); }
	|
	;

/* LogProtoSource related options */
source_proto_option
        : KW_ENCODING '(' string ')'
          {
            CHECK_ERROR(log_proto_server_options_set_encoding(last_proto_server_options, $3),
                        @3,
                        "unknown encoding %s", $3);
            free($3);
          }
        | KW_LOG_MSG_SIZE '(' positive_integer ')'      { last_proto_server_options->max_msg_size = $3; }
        | KW_TRIM_LARGE_MESSAGES '(' yesno ')'          { last_proto_server_options->trim_large_messages = $3; }
        ;

host_resolve_option
        : KW_USE_FQDN '(' yesno ')'             { last_host_resolve_options->use_fqdn = $3; }
        | KW_USE_DNS '(' dnsmode ')'            { last_host_resolve_options->use_dns = $3; }
	| KW_DNS_CACHE '(' yesno ')' 		{ last_host_resolve_options->use_dns_cache = $3; }
	| KW_NORMALIZE_HOSTNAMES '(' yesno ')'	{ last_host_resolve_options->normalize_hostnames = $3; }
	;

msg_format_option
	: KW_TIME_ZONE '(' string ')'		{ last_msg_format_options->recv_time_zone = g_strdup($3); free($3); }
	| KW_DEFAULT_LEVEL '(' level_string ')'
	  {
	    if (last_msg_format_options->default_pri == 0xFFFF)
	      last_msg_format_options->default_pri = LOG_USER;
	    last_msg_format_options->default_pri = (last_msg_format_options->default_pri & ~7) | $3;
          }
	| KW_DEFAULT_FACILITY '(' facility_string ')'
	  {
	    if (last_msg_format_options->default_pri == 0xFFFF)
	      last_msg_format_options->default_pri = LOG_NOTICE;
	    last_msg_format_options->default_pri = (last_msg_format_options->default_pri & 7) | $3;
          }
        ;

dest_writer_options
	: dest_writer_option dest_writer_options
	|
	;

dest_writer_option
        /* NOTE: plugins need to set "last_writer_options" in order to incorporate this rule in their grammar */

	: KW_FLAGS '(' dest_writer_options_flags ')' { last_writer_options->options = $3; }
	| KW_FLUSH_LINES '(' nonnegative_integer ')'		{ last_writer_options->flush_lines = $3; }
	| KW_FLUSH_TIMEOUT '(' positive_integer ')'	{ }
        | KW_SUPPRESS '(' nonnegative_integer ')'            { last_writer_options->suppress = $3; }
	| KW_TEMPLATE '(' string ')'       	{
                                                  GError *error = NULL;

                                                  last_writer_options->template = cfg_tree_check_inline_template(&configuration->tree, $3, &error);
                                                  CHECK_ERROR_GERROR(last_writer_options->template != NULL, @3, error, "Error compiling template");
	                                          free($3);
	                                        }
	| KW_TEMPLATE_ESCAPE '(' yesno ')'	{ log_writer_options_set_template_escape(last_writer_options, $3); }
	| KW_PAD_SIZE '(' nonnegative_integer ')'         { last_writer_options->padding = $3; }
	| KW_MARK_FREQ '(' nonnegative_integer ')'        { last_writer_options->mark_freq = $3; }
        | KW_MARK_MODE '(' KW_INTERNAL ')'      { log_writer_options_set_mark_mode(last_writer_options, "internal"); }
	| KW_MARK_MODE '(' string ')'
	  {
	    CHECK_ERROR(cfg_lookup_mark_mode($3) != -1, @3, "illegal mark mode: %s", $3);
            log_writer_options_set_mark_mode(last_writer_options, $3);
            free($3);
          }
        | { last_template_options = &last_writer_options->template_options; } template_option
	;

dest_writer_options_flags
	: normalized_flag dest_writer_options_flags   { $$ = log_writer_options_lookup_flag($1) | $2; free($1); }
	|					      { $$ = 0; }
	;

file_perm_option
	: KW_OWNER '(' string_or_number ')'	{ file_perm_options_set_file_uid(last_file_perm_options, $3); free($3); }
	| KW_OWNER '(' ')'	                { file_perm_options_dont_change_file_uid(last_file_perm_options); }
	| KW_GROUP '(' string_or_number ')'	{ file_perm_options_set_file_gid(last_file_perm_options, $3); free($3); }
	| KW_GROUP '(' ')'	                { file_perm_options_dont_change_file_gid(last_file_perm_options); }
	| KW_PERM '(' LL_NUMBER ')'		{ file_perm_options_set_file_perm(last_file_perm_options, $3); }
	| KW_PERM '(' ')'		        { file_perm_options_dont_change_file_perm(last_file_perm_options); }
        | KW_DIR_OWNER '(' string_or_number ')'	{ file_perm_options_set_dir_uid(last_file_perm_options, $3); free($3); }
	| KW_DIR_OWNER '(' ')'	                { file_perm_options_dont_change_dir_uid(last_file_perm_options); }
	| KW_DIR_GROUP '(' string_or_number ')'	{ file_perm_options_set_dir_gid(last_file_perm_options, $3); free($3); }
	| KW_DIR_GROUP '(' ')'	                { file_perm_options_dont_change_dir_gid(last_file_perm_options); }
	| KW_DIR_PERM '(' LL_NUMBER ')'		{ file_perm_options_set_dir_perm(last_file_perm_options, $3); }
	| KW_DIR_PERM '(' ')'		        { file_perm_options_dont_change_dir_perm(last_file_perm_options); }
        ;

template_option
	: KW_TS_FORMAT '(' string ')'		{ last_template_options->ts_format = cfg_ts_format_value($3); free($3); }
	| KW_FRAC_DIGITS '(' nonnegative_integer ')'	{ last_template_options->frac_digits = $3; }
	| KW_TIME_ZONE '(' string ')'		{ last_template_options->time_zone[LTZ_SEND] = g_strdup($3); free($3); }
	| KW_SEND_TIME_ZONE '(' string ')'      { last_template_options->time_zone[LTZ_SEND] = g_strdup($3); free($3); }
	| KW_LOCAL_TIME_ZONE '(' string ')'     { last_template_options->time_zone[LTZ_LOCAL] = g_strdup($3); free($3); }
	| KW_ON_ERROR '(' string ')'
        {
          gint on_error;

          CHECK_ERROR(log_template_on_error_parse($3, &on_error), @3, "Invalid on-error() setting");
          free($3);

          log_template_options_set_on_error(last_template_options, on_error);
        }
	;

matcher_option
        : KW_TYPE '(' string ')'		{ CHECK_ERROR(log_matcher_options_set_type(last_matcher_options, $3), @3, "unknown matcher type"); free($3); }
        | KW_FLAGS '(' matcher_flags ')'
        ;

matcher_flags
        : string matcher_flags			{ CHECK_ERROR(log_matcher_options_process_flag(last_matcher_options, $1), @1, "unknown matcher flag"); free($1); }
        |
        ;

value_pair_option
	: KW_VALUE_PAIRS
          {
            last_value_pairs = value_pairs_new();
          }
          '(' vp_options ')'
          { $$ = last_value_pairs; }
	;

vp_options
	: vp_option vp_options
	|
	;

vp_option
        : KW_PAIR '(' string ':' template_content ')'
          {
            value_pairs_add_pair(last_value_pairs, $3, $5);
            free($3);
          }
        | KW_PAIR '(' string template_content ')'
          {
            value_pairs_add_pair(last_value_pairs, $3, $4);
            free($3);
          }
        | KW_KEY '(' string KW_REKEY '('
          {
            last_vp_transset = value_pairs_transform_set_new($3);
            value_pairs_add_glob_pattern(last_value_pairs, $3, TRUE);
            free($3);
          }
          vp_rekey_options ')'                           { value_pairs_add_transforms(last_value_pairs, last_vp_transset); } ')'
	| KW_KEY '(' string_list ')'		         { value_pairs_add_glob_patterns(last_value_pairs, $3, TRUE); }
        | KW_REKEY '(' string
          {
            last_vp_transset = value_pairs_transform_set_new($3);
            free($3);
          }
          vp_rekey_options ')'                           { value_pairs_add_transforms(last_value_pairs, last_vp_transset); }
        | KW_EXCLUDE '(' string_list ')'                 { value_pairs_add_glob_patterns(last_value_pairs, $3, FALSE); }
	| KW_SCOPE '(' vp_scope_list ')'
	;

vp_scope_list
	: string vp_scope_list                           { value_pairs_add_scope(last_value_pairs, $1); free($1); }
	|
	;

vp_rekey_options
	: vp_rekey_option vp_rekey_options
        |
	;

vp_rekey_option
	: KW_SHIFT '(' positive_integer ')' { value_pairs_transform_set_add_func(last_vp_transset, value_pairs_new_transform_shift($3)); }
	| KW_SHIFT_LEVELS '(' positive_integer ')' { value_pairs_transform_set_add_func(last_vp_transset, value_pairs_new_transform_shift_levels($3)); }
	| KW_ADD_PREFIX '(' string ')' { value_pairs_transform_set_add_func(last_vp_transset, value_pairs_new_transform_add_prefix($3)); free($3); }
	| KW_REPLACE_PREFIX '(' string string ')' { value_pairs_transform_set_add_func(last_vp_transset, value_pairs_new_transform_replace_prefix($3, $4)); free($3); free($4); }
	;

rewrite_expr_opt
        : KW_VALUE '(' string ')'
          {
            const gchar *p = $3;
            if (p[0] == '$')
              {
                msg_warning("Value references in rewrite rules should not use the '$' prefix, those are only needed in templates",
                            evt_tag_str("value", $3),
                            cfg_lexer_format_location_tag(lexer, &@3));
                p++;
              }
            last_rewrite->value_handle = log_msg_get_value_handle(p);
            CHECK_ERROR(!log_msg_is_handle_macro(last_rewrite->value_handle), @3, "%s is read-only, it cannot be changed in rewrite rules", p);
	    CHECK_ERROR(log_msg_is_value_name_valid(p), @3, "%s is not a valid name for a name-value pair, perhaps a misspelled .SDATA reference?", p);
            free($3);
          }
        | rewrite_condition_opt
        ;

rewrite_condition_opt
        : KW_CONDITION '('
          {
            FilterExprNode *filter_expr;

            CHECK_ERROR_WITHOUT_MESSAGE(cfg_parser_parse(&filter_expr_parser, lexer, (gpointer *) &filter_expr, NULL), @1);
            log_rewrite_set_condition(last_rewrite, filter_expr);
          } ')'
        ;


/* END_RULES */


%%
