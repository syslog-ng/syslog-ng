%{

#include "syslog-ng.h"
#include "cfg.h"
#include "sgroup.h"
#include "dgroup.h"
#include "center.h"
#include "filter.h"
#include "templates.h"
#include "logreader.h"
#include "logparser.h"
#include "logrewrite.h"

#if ENABLE_SSL /* BEGIN MARK: tls */
#include "tlscontext.h"
#endif         /* END MARK */

#include "affile.h"
#include "afinter.h"
#include "afsocket.h"
#include "afinet.h"
#include "afunix.h"
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

/* FIXME: the lexer allocates strings with strdup instead of g_strdup,
 * therefore there are unnecessary g_strdup/free pairs in the grammar. These
 * should be removed. */

void yyerror(char *msg);
int yylex();

LogDriver *last_driver;
LogReaderOptions *last_reader_options;
LogWriterOptions *last_writer_options;
LogTemplate *last_template;
SocketOptions *last_sock_options;
LogParser *last_parser;
FilterRE *last_re_filter;
LogRewrite *last_rewrite;
gint last_addr_family = AF_INET;
gchar *last_include_file;

#if ENABLE_SSL
TLSContext *last_tls_context;
#endif


#if ! ENABLE_IPV6
#undef AF_INET6
#define AF_INET6 0; g_assert_not_reached()

#endif

static struct _LogTemplate *
cfg_check_inline_template(GlobalConfig *cfg, const gchar *template_or_name)
{
  struct _LogTemplate *template = cfg_lookup_template(configuration, template_or_name);
  if (template == NULL)
    {
      template = log_template_new(NULL, template_or_name); 
      template->def_inline = TRUE;
    }
  return template;
}

static gboolean
cfg_check_template(LogTemplate *template)
{
  GError *error = NULL;
  if (!log_template_compile(template, &error))
    {
      msg_error("Error compiling template",
                evt_tag_str("template", template->template),
                evt_tag_str("error", error->message),
                NULL);
      g_clear_error(&error);
      return FALSE;
    }
  return TRUE;
}


%}

%union {
        gint token;
	gint64 num;
	double fnum;
	char *cptr;
	void *ptr;
	FilterExprNode *node;
}

/* statements */
%token	KW_SOURCE KW_FILTER KW_PARSER KW_DESTINATION KW_LOG KW_OPTIONS KW_INCLUDE

/* source & destination items */
%token	KW_INTERNAL KW_FILE KW_PIPE KW_UNIX_STREAM KW_UNIX_DGRAM
%token  KW_TCP KW_UDP KW_TCP6 KW_UDP6
%token  KW_USERTTY KW_DOOR KW_SUN_STREAMS KW_PROGRAM
%token  KW_SQL KW_TYPE KW_COLUMNS KW_INDEXES KW_VALUES KW_PASSWORD KW_DATABASE KW_USERNAME KW_TABLE KW_ENCODING
%token  KW_DELIMITERS KW_QUOTES KW_QUOTE_PAIRS KW_NULL
%token  KW_SYSLOG KW_TRANSPORT

/* option items */
%token KW_FSYNC KW_MARK_FREQ KW_STATS_FREQ KW_STATS_LEVEL KW_FLUSH_LINES KW_SUPPRESS KW_FLUSH_TIMEOUT KW_LOG_MSG_SIZE KW_FILE_TEMPLATE KW_PROTO_TEMPLATE

%token KW_CHAIN_HOSTNAMES KW_NORMALIZE_HOSTNAMES KW_KEEP_HOSTNAME KW_CHECK_HOSTNAME KW_BAD_HOSTNAME
%token KW_KEEP_TIMESTAMP
%token KW_USE_DNS KW_USE_FQDN 
%token KW_DNS_CACHE KW_DNS_CACHE_SIZE
%token KW_DNS_CACHE_EXPIRE KW_DNS_CACHE_EXPIRE_FAILED KW_DNS_CACHE_HOSTS
%token KW_PERSIST_ONLY
%token KW_TZ_CONVERT KW_TS_FORMAT KW_FRAC_DIGITS

%token KW_LOG_FIFO_SIZE KW_LOG_DISK_FIFO_SIZE KW_LOG_FETCH_LIMIT KW_LOG_IW_SIZE KW_LOG_PREFIX KW_PROGRAM_OVERRIDE KW_HOST_OVERRIDE
%token KW_THROTTLE

/* SSL support */
%token KW_TLS KW_PEER_VERIFY KW_KEY_FILE KW_CERT_FILE KW_CA_DIR KW_CRL_DIR KW_TRUSTED_KEYS KW_TRUSTED_DN

/* log statement options */
%token KW_FLAGS

/* reader options */
%token KW_PAD_SIZE KW_TIME_ZONE KW_RECV_TIME_ZONE KW_SEND_TIME_ZONE KW_LOCAL_TIME_ZONE

/* timers */
%token KW_TIME_REOPEN KW_TIME_REAP KW_TIME_SLEEP 

/* destination options */
%token KW_TMPL_ESCAPE

/* driver specific options */
%token KW_OPTIONAL

/* file related options */
%token KW_CREATE_DIRS 
%token KW_OWNER KW_GROUP KW_PERM 
%token KW_DIR_OWNER KW_DIR_GROUP KW_DIR_PERM 
%token KW_TEMPLATE KW_TEMPLATE_ESCAPE
%token KW_FOLLOW_FREQ
%token KW_OVERWRITE_IF_OLDER
%token KW_DEFAULT_FACILITY KW_DEFAULT_LEVEL

/* socket related options */
%token KW_KEEP_ALIVE KW_MAX_CONNECTIONS
%token KW_LOCALIP KW_IP KW_LOCALPORT KW_PORT KW_DESTPORT
%token KW_IP_TTL KW_SO_BROADCAST KW_IP_TOS KW_SO_SNDBUF KW_SO_RCVBUF KW_SO_KEEPALIVE KW_SPOOF_SOURCE

/* misc options */
%token KW_USE_TIME_RECVD

/* filter items*/
%token KW_FACILITY KW_LEVEL KW_HOST KW_MATCH KW_MESSAGE KW_NETMASK

/* parser items */
%token KW_CSV_PARSER KW_VALUE KW_DB_PARSER

/* rewrite items */
%token KW_REWRITE KW_SET KW_SUBST

/* yes/no switches */
%token KW_YES KW_NO

/* obsolete, compatibility and not-yet supported options */
%token KW_GC_IDLE_THRESHOLD KW_GC_BUSY_THRESHOLD  
%token KW_COMPRESS KW_MAC KW_AUTH KW_ENCRYPT

%token KW_IFDEF
%token KW_ENDIF

%token  LL_DOTDOT
%token	<cptr> LL_IDENTIFIER
%token	<num>  LL_NUMBER
%token	<fnum> LL_FLOAT
%token	<cptr> LL_STRING

%left	KW_OR
%left	KW_AND
%left   KW_NOT

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
%type	<ptr> source_afsocket
%type	<ptr> source_afunix_dgram_params
%type	<ptr> source_afunix_stream_params
%type	<ptr> source_afinet_udp_params
%type	<ptr> source_afinet_tcp_params
%type   <ptr> source_afsyslog
%type   <ptr> source_afsyslog_params
%type   <ptr> source_afsocket_stream_params
%type	<ptr> source_afstreams
%type	<ptr> source_afstreams_params
%type	<num> source_reader_option_flags
%type   <ptr> source_afprogram
%type   <ptr> source_afprogram_params

%type	<ptr> dest_items
%type	<ptr> dest_item
%type   <ptr> dest_affile
%type	<ptr> dest_affile_params
%type   <ptr> dest_afpipe
%type   <ptr> dest_afpipe_params
%type	<ptr> dest_afsocket
%type	<ptr> dest_afunix_dgram_params
%type	<ptr> dest_afunix_stream_params
%type	<ptr> dest_afinet_udp_params
%type	<ptr> dest_afinet_tcp_params
%type   <ptr> dest_afsyslog
%type   <ptr> dest_afsyslog_params
%type   <ptr> dest_afuser
%type   <ptr> dest_afprogram
%type   <ptr> dest_afprogram_params
/* BEGIN MARK: sql */
%type   <ptr> dest_afsql
%type   <ptr> dest_afsql_params
/* END MARK */
%type	<num> dest_writer_options_flags

%type	<ptr> log_items
%type	<ptr> log_item

%type   <ptr> log_forks
%type   <ptr> log_fork

%type	<num> log_flags
%type   <num> log_flags_items

%type	<ptr> options_items
%type	<ptr> options_item

%type	<node> filter_expr
%type	<node> filter_simple_expr

%type   <num> filter_fac_list
%type	<num> filter_level_list
%type	<num> filter_level

%type	<ptr> parser_expr
%type   <num> parser_csv_flags

%type   <ptr> rewrite_expr
%type   <ptr> rewrite_expr_list
%type   <ptr> rewrite_expr_list_build

%type   <num> regexp_option_flags

%type	<num> yesno
%type   <num> dnsmode

%type	<cptr> string
%type	<cptr> string_or_number
%type   <ptr> string_list
%type   <ptr> string_list_build
%type   <num> facility_string
%type   <num> level_string

%type   <token> reserved_words_as_strings

%type <token> KW_PARSER
%type <token> KW_REWRITE
%type <token> KW_INCLUDE
%type <token> KW_SYSLOG
%type <token> KW_COLUMNS
%type <token> KW_DELIMITERS
%type <token> KW_QUOTES
%type <token> KW_QUOTE_PAIRS
%type <token> KW_NULL
%type <token> KW_CSV_PARSER
%type <token> KW_DB_PARSER
%type <token> KW_ENCODING
%type <token> KW_SET
%type <token> KW_SUBST
%type <token> KW_VALUE
%type <token> KW_PROGRAM_OVERRIDE
%type <token> KW_HOST_OVERRIDE
%type <token> KW_TRANSPORT
%type <token> KW_TRUSTED_KEYS
%type <token> KW_TRUSTED_DN
%type <token> KW_MESSAGE
%type <token> KW_TYPE
%type <token> KW_SQL
%type <token> KW_DEFAULT_FACILITY
%type <token> KW_DEFAULT_LEVEL

%%

start   
        : stmts
	;

stmts   
        : stmt ';' 
          { 
            if (last_include_file && !cfg_lex_process_include(last_include_file)) 
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
	: string '{' filter_expr ';' '}'	{ $$ = log_filter_rule_new($1, $3); free($1); }
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

socket_option
	: KW_SO_SNDBUF '(' LL_NUMBER ')'           { last_sock_options->sndbuf = $3; }
	| KW_SO_RCVBUF '(' LL_NUMBER ')'           { last_sock_options->rcvbuf = $3; }
	| KW_SO_BROADCAST '(' yesno ')'         { last_sock_options->broadcast = $3; }
	| KW_SO_KEEPALIVE '(' yesno ')'         { last_sock_options->keepalive = $3; }
	;

inet_socket_option
	: socket_option
	| KW_IP_TTL '(' LL_NUMBER ')'              { ((InetSocketOptions *) last_sock_options)->ttl = $3; }
	| KW_IP_TOS '(' LL_NUMBER ')'              { ((InetSocketOptions *) last_sock_options)->tos = $3; }
	;

source_items
        : source_item ';' source_items		{ if ($1) {log_drv_append($1, $3); log_drv_unref($3); $$ = $1; } else { YYERROR; } }
	|					{ $$ = NULL; }
	;

source_item
  	: source_afinter			{ $$ = $1; }
	| source_affile				{ $$ = $1; }
	| source_afsocket			{ $$ = $1; }
	| source_afsyslog			{ $$ = $1; }
	| source_afstreams			{ $$ = $1; }
        | source_afprogram                      { $$ = $1; }
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

source_afsocket
	: KW_UNIX_DGRAM '(' source_afunix_dgram_params ')'	{ $$ = $3; }
	| KW_UNIX_STREAM '(' source_afunix_stream_params ')' 	{ $$ = $3; }
	| KW_UDP { last_addr_family = AF_INET; } '(' source_afinet_udp_params ')'		{ $$ = $4; } 
	| KW_TCP { last_addr_family = AF_INET; } '(' source_afinet_tcp_params ')'		{ $$ = $4; } 
	| KW_UDP6 { last_addr_family = AF_INET6; } '(' source_afinet_udp_params ')'		{ $$ = $4; } 
	| KW_TCP6 { last_addr_family = AF_INET6; } '(' source_afinet_tcp_params ')'		{ $$ = $4; } 
	;
 	
source_afunix_dgram_params
	: string 
	  { 
	    last_driver = afunix_sd_new(
		$1,
		AFSOCKET_DGRAM | AFSOCKET_LOCAL); 
	    free($1); 
	    last_reader_options = &((AFSocketSourceDriver *) last_driver)->reader_options;
	    last_sock_options = &((AFUnixSourceDriver *) last_driver)->sock_options;
	  }
	  source_afunix_options			{ $$ = last_driver; }
	;
	
source_afunix_stream_params
	: string 
	  { 
	    last_driver = afunix_sd_new(
		$1,
		AFSOCKET_STREAM | AFSOCKET_KEEP_ALIVE | AFSOCKET_LOCAL);
	    free($1);
	    last_reader_options = &((AFSocketSourceDriver *) last_driver)->reader_options;
	    last_sock_options = &((AFUnixSourceDriver *) last_driver)->sock_options;
	  }
	  source_afunix_options			{ $$ = last_driver; }
	;

/* options are common between dgram & stream */
source_afunix_options
	: source_afunix_option source_afunix_options
	|
	;

source_afunix_option
	: KW_OWNER '(' string_or_number ')'	{ afunix_sd_set_uid(last_driver, $3); free($3); }
	| KW_GROUP '(' string_or_number ')'	{ afunix_sd_set_gid(last_driver, $3); free($3); }
	| KW_PERM '(' LL_NUMBER ')'		{ afunix_sd_set_perm(last_driver, $3); }
	| KW_OPTIONAL '(' yesno ')'		{ last_driver->optional = $3; }
	| source_afsocket_stream_params		{}
	| source_reader_option			{}
	| socket_option				{}
	; 

source_afinet_udp_params
        : 
          { 
	    last_driver = afinet_sd_new(last_addr_family,
			NULL, 514,
			AFSOCKET_DGRAM);
	    last_reader_options = &((AFSocketSourceDriver *) last_driver)->reader_options;
	    last_sock_options = &((AFInetSourceDriver *) last_driver)->sock_options.super;
	  }
	  source_afinet_udp_options		{ $$ = last_driver; }
	;

source_afinet_udp_options
	: source_afinet_udp_option source_afinet_udp_options
	|
	;

source_afinet_udp_option
	: source_afinet_option
	;

source_afinet_option
	: KW_LOCALIP '(' string ')'		{ afinet_sd_set_localip(last_driver, $3); free($3); }
	| KW_IP '(' string ')'			{ afinet_sd_set_localip(last_driver, $3); free($3); }
	| KW_LOCALPORT '(' string_or_number ')'	{ afinet_sd_set_localport(last_driver, $3, afinet_sd_get_proto_name(last_driver)); free($3); }
	| KW_PORT '(' string_or_number ')'	{ afinet_sd_set_localport(last_driver, $3, afinet_sd_get_proto_name(last_driver)); free($3); }
	| source_reader_option
	| inet_socket_option
	;

source_afinet_tcp_params
	: 
	  { 
	    last_driver = afinet_sd_new(last_addr_family,
			NULL, 514,
			AFSOCKET_STREAM);
	    last_reader_options = &((AFSocketSourceDriver *) last_driver)->reader_options;
	    last_sock_options = &((AFInetSourceDriver *) last_driver)->sock_options.super;
	  }
	  source_afinet_tcp_options	{ $$ = last_driver; }
	;

source_afinet_tcp_options
	: source_afinet_tcp_option source_afinet_tcp_options
	|
	;

source_afinet_tcp_option
        : source_afinet_option
/* BEGIN MARK: tls */
	| KW_TLS 
	  {
#if ENABLE_SSL
	    last_tls_context = tls_context_new(TM_SERVER);
#endif
	  }
	  '(' tls_options ')'			
	  { 
#if ENABLE_SSL
	    afsocket_sd_set_tls_context(last_driver, last_tls_context); 
#endif
          }
/* END MARK */
	| source_afsocket_stream_params		{}
	;

source_afsocket_stream_params
	: KW_KEEP_ALIVE '(' yesno ')'		{ afsocket_sd_set_keep_alive(last_driver, $3); }
	| KW_MAX_CONNECTIONS '(' LL_NUMBER ')'	{ afsocket_sd_set_max_connections(last_driver, $3); }
	;

source_afsyslog	
	: KW_SYSLOG { last_addr_family = AF_INET; } '(' source_afsyslog_params ')'		{ $$ = $4; } 
	;
	
source_afsyslog_params
	: 
	  { 
	    last_driver = afinet_sd_new(last_addr_family,
			NULL, 601,
			AFSOCKET_STREAM | AFSOCKET_SYSLOG_PROTOCOL);
	    last_reader_options = &((AFSocketSourceDriver *) last_driver)->reader_options;
	    last_sock_options = &((AFInetSourceDriver *) last_driver)->sock_options.super;
	  }
	  source_afsyslog_options	{ $$ = last_driver; }
	;

source_afsyslog_options
	: source_afsyslog_option source_afsyslog_options
	|
	;

source_afsyslog_option
        : source_afinet_option
        | KW_TRANSPORT '(' string ')'           { afinet_sd_set_transport(last_driver, $3); free($3); }
        | KW_TRANSPORT '(' KW_TCP ')'           { afinet_sd_set_transport(last_driver, "tcp"); }
        | KW_TRANSPORT '(' KW_UDP ')'           { afinet_sd_set_transport(last_driver, "udp"); }
        | KW_TRANSPORT '(' KW_TLS ')'           { afinet_sd_set_transport(last_driver, "tls"); }
	| KW_TLS 
	  {
#if ENABLE_SSL
	    last_tls_context = tls_context_new(TM_SERVER);
#endif
	  }
	  '(' tls_options ')'			
	  { 
#if ENABLE_SSL
	    afsocket_sd_set_tls_context(last_driver, last_tls_context); 
#endif
          }
	| source_afsocket_stream_params		{}
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


dest_items
	: dest_item ';' dest_items		{ log_drv_append($1, $3); log_drv_unref($3); $$ = $1; }
	|					{ $$ = NULL; }
	;

dest_item
	: dest_affile				{ $$ = $1; }
	| dest_afpipe				{ $$ = $1; }
	| dest_afsocket				{ $$ = $1; }
	| dest_afuser				{ $$ = $1; }
	| dest_afprogram			{ $$ = $1; }
	| dest_afsyslog                         { $$ = $1; }
/* BEGIN MARK: sql */
	| dest_afsql				{ $$ = $1; }
/* END MARK */
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

dest_afsocket
	: KW_UNIX_DGRAM '(' dest_afunix_dgram_params ')'	{ $$ = $3; }
	| KW_UNIX_STREAM '(' dest_afunix_stream_params ')'	{ $$ = $3; }
	| KW_UDP { last_addr_family = AF_INET; } '(' dest_afinet_udp_params ')'			{ $$ = $4; }
	| KW_TCP { last_addr_family = AF_INET; } '(' dest_afinet_tcp_params ')'			{ $$ = $4; } 
	| KW_UDP6 { last_addr_family = AF_INET6; } '(' dest_afinet_udp_params ')'			{ $$ = $4; }
	| KW_TCP6 { last_addr_family = AF_INET6; } '(' dest_afinet_tcp_params ')'			{ $$ = $4; } 
	;

dest_afunix_dgram_params
	: string				
	  { 
	    last_driver = afunix_dd_new($1, AFSOCKET_DGRAM);
	    free($1);
	    last_writer_options = &((AFSocketDestDriver *) last_driver)->writer_options;
	    last_sock_options = &((AFUnixDestDriver *) last_driver)->sock_options;
	  }
	  dest_afunix_options			{ $$ = last_driver; }
	;

dest_afunix_stream_params
	: string				
	  { 
	    last_driver = afunix_dd_new($1, AFSOCKET_STREAM);
	    free($1);
	    last_writer_options = &((AFSocketDestDriver *) last_driver)->writer_options;
	    last_sock_options = &((AFUnixDestDriver *) last_driver)->sock_options;
	  }
	  dest_afunix_options			{ $$ = last_driver; }
	;

dest_afunix_options
	: dest_afunix_options dest_afunix_option
	|
	;

dest_afunix_option
	: dest_writer_option
	| dest_afsocket_option
	| socket_option
	;

dest_afinet_udp_params
	: string 	
	  { 
	    last_driver = afinet_dd_new(last_addr_family,
			$1, 514,
			AFSOCKET_DGRAM);
	    free($1);
	    last_writer_options = &((AFSocketDestDriver *) last_driver)->writer_options;
	    last_sock_options = &((AFInetDestDriver *) last_driver)->sock_options.super;
	  }
	  dest_afinet_udp_options		{ $$ = last_driver; }
	;

dest_afinet_udp_options
        : dest_afinet_udp_options dest_afinet_udp_option
	|
	;


dest_afinet_option
	: KW_LOCALIP '(' string ')'		{ afinet_dd_set_localip(last_driver, $3); free($3); }
	| KW_LOCALPORT '(' string_or_number ')'	{ afinet_dd_set_localport(last_driver, $3, afinet_dd_get_proto_name(last_driver)); free($3); }
	| KW_PORT '(' string_or_number ')'	{ afinet_dd_set_destport(last_driver, $3, afinet_dd_get_proto_name(last_driver)); free($3); }
	| KW_DESTPORT '(' string_or_number ')'	{ afinet_dd_set_destport(last_driver, $3, afinet_dd_get_proto_name(last_driver)); free($3); }
	| inet_socket_option
	| dest_writer_option
	| dest_afsocket_option
	;

dest_afinet_udp_option
	: dest_afinet_option
	| KW_SPOOF_SOURCE '(' yesno ')'		{ afinet_dd_set_spoof_source(last_driver, $3); }
	;

dest_afinet_tcp_params
	: string 	
	  { 
	    last_driver = afinet_dd_new(last_addr_family,
			$1, 514,
			AFSOCKET_STREAM); 
	    free($1);
	    last_writer_options = &((AFSocketDestDriver *) last_driver)->writer_options;
	    last_sock_options = &((AFInetDestDriver *) last_driver)->sock_options.super;
	  }
	  dest_afinet_tcp_options		{ $$ = last_driver; }
	;

dest_afinet_tcp_options
	: dest_afinet_tcp_options dest_afinet_tcp_option
	|
	;

dest_afinet_tcp_option
	: dest_afinet_option
	| KW_TLS 
	  {
#if ENABLE_SSL
	    last_tls_context = tls_context_new(TM_CLIENT);
#endif
	  }
	  '(' tls_options ')'			
	  { 
#if ENABLE_SSL
	    afsocket_dd_set_tls_context(last_driver, last_tls_context); 
#endif
          }
	;
	
dest_afsocket_option
        : KW_KEEP_ALIVE '(' yesno ')'        { afsocket_dd_set_keep_alive(last_driver, $3); }
        ;


dest_afsyslog
        : KW_SYSLOG '(' dest_afsyslog_params ')'   { $$ = $3; }

dest_afsyslog_params
        : string
          {
            last_driver = afinet_dd_new(last_addr_family, $1, 601, AFSOCKET_STREAM | AFSOCKET_SYSLOG_PROTOCOL);
	    last_writer_options = &((AFSocketDestDriver *) last_driver)->writer_options;
	    last_sock_options = &((AFInetDestDriver *) last_driver)->sock_options.super;
	    free($1);
	  }
	  dest_afsyslog_options			{ $$ = last_driver; }
        ;


dest_afsyslog_options
	: dest_afsyslog_options dest_afsyslog_option
	|
	;

dest_afsyslog_option
	: dest_afinet_option
        | KW_TRANSPORT '(' string ')'           { afinet_dd_set_transport(last_driver, $3); free($3); }
        | KW_TRANSPORT '(' KW_TCP ')'           { afinet_dd_set_transport(last_driver, "tcp"); }
        | KW_TRANSPORT '(' KW_UDP ')'           { afinet_dd_set_transport(last_driver, "udp"); }
        | KW_TRANSPORT '(' KW_TLS ')'           { afinet_dd_set_transport(last_driver, "tls"); }
	| KW_SPOOF_SOURCE '(' yesno ')'		{ afinet_dd_set_spoof_source(last_driver, $3); }
	| KW_TLS 
	  {
#if ENABLE_SSL
	    last_tls_context = tls_context_new(TM_CLIENT);
#endif
	  }
	  '(' tls_options ')'			
	  { 
#if ENABLE_SSL
	    afsocket_dd_set_tls_context(last_driver, last_tls_context); 
#endif
          }
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
	
/* BEGIN MARK: sql */

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

        | KW_ENDIF { 
#endif /* ENABLE_SQL */ 
}
        ;
/* END MARK */


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

/* BEGIN MARK: tls */
tls_options
	: tls_option tls_options
	|
	;

tls_option
        : KW_IFDEF { 
#if ENABLE_SSL
} 

	| KW_PEER_VERIFY '(' string ')'		
	  { 
	    last_tls_context->verify_mode = tls_lookup_verify_mode($3); 
            free($3); 
          }
	| KW_KEY_FILE '(' string ')'		
	  { 
	    last_tls_context->key_file = g_strdup($3); 
            free($3);
          }
	| KW_CERT_FILE '(' string ')'		
	  { 
	    last_tls_context->cert_file = g_strdup($3); 
            free($3);
          }
	| KW_CA_DIR '(' string ')'		
	  { 
	    last_tls_context->ca_dir = g_strdup($3); 
            free($3);
          }
	| KW_CRL_DIR '(' string ')'		
	  { 
	    last_tls_context->crl_dir = g_strdup($3); 
            free($3);
          }
        | KW_TRUSTED_KEYS '(' string_list ')' 
          { 
            tls_session_set_trusted_fingerprints(last_tls_context, $3); 
          }
        | KW_TRUSTED_DN '(' string_list ')' 
          { 
            tls_session_set_trusted_dn(last_tls_context, $3); 
          }
        | KW_ENDIF {
#endif
} 
        ;
/* END MARK */


filter_expr
	: filter_simple_expr			{ $$ = $1; if (!$1) return 1; }
        | KW_NOT filter_expr			{ $2->comp = !($2->comp); $$ = $2; }
	| filter_expr KW_OR filter_expr		{ $$ = fop_or_new($1, $3); }
	| filter_expr KW_AND filter_expr	{ $$ = fop_and_new($1, $3); }
	| '(' filter_expr ')'			{ $$ = $2; }
	;

filter_simple_expr
	: KW_FACILITY '(' filter_fac_list ')'	{ $$ = filter_facility_new($3);  }
	| KW_FACILITY '(' LL_NUMBER ')'		{ $$ = filter_facility_new(0x80000000 | $3); }
	| KW_LEVEL '(' filter_level_list ')' 	{ $$ = filter_level_new($3); }
	| KW_FILTER '(' string ')'		{ $$ = filter_call_new($3, configuration); free($3); }
	| KW_NETMASK '(' string ')'		{ $$ = filter_netmask_new($3); free($3); }
	| KW_PROGRAM '(' string
	  { 
	    last_re_filter = (FilterRE *) filter_re_new(LOG_MESSAGE_BUILTIN_FIELD(PROGRAM)); 
          }
          filter_re_opts ')'  
          {
            if(!filter_re_set_regexp(last_re_filter, $3))
              YYERROR;
            free($3); 

            $$ = &last_re_filter->super;
          }
	| KW_HOST '(' string
	  {
	    last_re_filter = (FilterRE *) filter_re_new(LOG_MESSAGE_BUILTIN_FIELD(HOST)); 
          }
          filter_re_opts ')'  
          {
            if(!filter_re_set_regexp(last_re_filter, $3))
              YYERROR;
            free($3); 
            
            $$ = &last_re_filter->super;
          }
	| KW_MATCH '(' string
	  { 
	    last_re_filter = (FilterRE *) filter_match_new(); 
	  }
          filter_match_opts ')'  
          {
            if(!filter_re_set_regexp(last_re_filter, $3))
              YYERROR;
            free($3); 
            $$ = &last_re_filter->super;
            
            if (last_re_filter->value_name == 0)
              {
                static gboolean warn_written = FALSE;
                
                if (!warn_written)
                  {
                    msg_warning("WARNING: the match() filter without the use of the value() option is deprecated and hinders performance, please update your configuration",
                                NULL);
                    warn_written = TRUE;
                  }
              }
          }
        | KW_MESSAGE '(' string           
          {
	    last_re_filter = (FilterRE *) filter_re_new(LOG_MESSAGE_BUILTIN_FIELD(MESSAGE)); 
          }
          filter_re_opts ')'  
          {
            if(!filter_re_set_regexp(last_re_filter, $3))
              YYERROR;
	    free($3); 
            $$ = &last_re_filter->super;
          }
        | KW_SOURCE '(' string           
          {
	    last_re_filter = (FilterRE *) filter_re_new(LOG_MESSAGE_BUILTIN_FIELD(SOURCE)); 
            filter_re_set_matcher(last_re_filter, log_matcher_string_new());
          }
          filter_re_opts ')'  
          {
            if(!filter_re_set_regexp(last_re_filter, $3))
              YYERROR;
	    free($3); 
            $$ = &last_re_filter->super;
          }
	;
	
filter_match_opts
        : filter_match_opt filter_match_opts
        |
        ;
        
filter_match_opt
        : filter_re_opt
        | KW_VALUE '(' string ')'               { last_re_filter->value_name = log_msg_translate_value_name($3); free($3); }
        ;
	
filter_re_opts 
        : filter_re_opt filter_re_opts
        |
        ;

filter_re_opt
        : KW_TYPE '(' string ')'                
          { 
            filter_re_set_matcher(last_re_filter, log_matcher_new($3));
            free($3); 
          }
        | KW_FLAGS '(' regexp_option_flags ')' { filter_re_set_flags(last_re_filter, $3); }
        ;

regexp_option_flags
        : string regexp_option_flags            { $$ = log_matcher_lookup_flag($1) | $2; free($1); }
        |                                       { $$ = 0; }
        ;


filter_fac_list
	: facility_string filter_fac_list	{ $$ = (1 << ($1 >> 3)) | $2; }
	| facility_string			{ $$ = (1 << ($1 >> 3)); }
	;

filter_level_list
	: filter_level filter_level_list	{ $$ = $1 | $2; }
	| filter_level				{ $$ = $1; }
	;

filter_level
	: level_string LL_DOTDOT level_string
	  { 
	    $$ = syslog_make_range($1, $3);
	  }
	| level_string
	  { 
	    $$ = 1 << $1;
	  }
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
        : KW_VALUE '(' string ')'               { last_rewrite->value_name = log_msg_translate_value_name($3); free($3); }
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

yesno
	: KW_YES				{ $$ = 1; }
	| KW_NO					{ $$ = 0; }
	| LL_NUMBER				{ $$ = $1; }
	;

dnsmode
	: yesno					{ $$ = $1; }
	| KW_PERSIST_ONLY                       { $$ = 2; }
	;

string
	: LL_IDENTIFIER
	| LL_STRING
	| reserved_words_as_strings             { $$ = cfg_lex_get_keyword_string($1); }
	;

reserved_words_as_strings
        /* these keywords were introduced in syslog-ng 3.0 */
        : KW_PARSER
        | KW_REWRITE
        | KW_INCLUDE
        | KW_COLUMNS
        | KW_DELIMITERS
        | KW_QUOTES
        | KW_QUOTE_PAIRS
        | KW_NULL
        | KW_CSV_PARSER
        | KW_DB_PARSER
        | KW_ENCODING
        | KW_SET
        | KW_SUBST
        | KW_VALUE
        | KW_PROGRAM_OVERRIDE
        | KW_HOST_OVERRIDE
        | KW_TRANSPORT
        | KW_TRUSTED_KEYS
        | KW_TRUSTED_DN
        | KW_MESSAGE
        | KW_TYPE
        | KW_SQL
        | KW_DEFAULT_FACILITY
        | KW_DEFAULT_LEVEL
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


%%

extern int linenum;

void 
yyerror(char *msg)
{
  fprintf(stderr, "%s in %s at line %d.\n\n"
                  "syslog-ng documentation: http://www.balabit.com/support/documentation/?product=syslog-ng\n"
                  "mailing list: https://lists.balabit.hu/mailman/listinfo/syslog-ng\n", msg, cfg_lex_get_current_file(), cfg_lex_get_current_lineno());
}

