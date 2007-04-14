%{

#include "syslog-ng.h"
#include "cfg.h"
#include "sgroup.h"
#include "dgroup.h"
#include "center.h"
#include "filter.h"
#include "templates.h"
#include "logreader.h"

#include "affile.h"
#include "afinter.h"
#include "afsocket.h"
#include "afinet.h"
#include "afunix.h"
#include "afstreams.h"
#include "afuser.h"
#include "afprog.h"

#include "messages.h"

#include "syslog-names.h"

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>

int lookup_parse_flag(char *flag);

void yyerror(char *msg);
int yylex();

LogDriver *last_driver;
LogReaderOptions *last_reader_options;
LogWriterOptions *last_writer_options;
LogTemplate *last_template;
SocketOptions *last_sock_options;
gint last_addr_family = AF_INET;

%}

%union {
	guint num;
	char *cptr;
	void *ptr;
	FilterExprNode *node;
}

/* statements */
%token	KW_SOURCE KW_DESTINATION KW_LOG KW_OPTIONS KW_FILTER

/* source & destination items */
%token	KW_INTERNAL KW_FILE KW_PIPE KW_UNIX_STREAM KW_UNIX_DGRAM
%token  KW_TCP KW_UDP KW_TCP6 KW_UDP6
%token  KW_USER KW_DOOR KW_SUN_STREAMS KW_PROGRAM

/* option items */
%token KW_FSYNC KW_MARK_FREQ KW_STATS_FREQ KW_FLUSH_LINES KW_FLUSH_TIMEOUT KW_LOG_MSG_SIZE KW_FILE_TEMPLATE KW_PROTO_TEMPLATE

%token KW_CHAIN_HOSTNAMES KW_NORMALIZE_HOSTNAMES KW_KEEP_HOSTNAME KW_CHECK_HOSTNAME KW_BAD_HOSTNAME
%token KW_KEEP_TIMESTAMP
%token KW_USE_DNS KW_USE_FQDN 
%token KW_DNS_CACHE KW_DNS_CACHE_SIZE
%token KW_DNS_CACHE_EXPIRE KW_DNS_CACHE_EXPIRE_FAILED KW_DNS_CACHE_HOSTS
%token KW_PERSIST_ONLY
%token KW_TZ_CONVERT KW_TS_FORMAT KW_FRAC_DIGITS

%token KW_LOG_FIFO_SIZE KW_LOG_FETCH_LIMIT KW_LOG_IW_SIZE KW_LOG_PREFIX

/* log statement options */
%token KW_FLAGS KW_CATCHALL KW_FALLBACK KW_FINAL KW_FLOW_CONTROL


/* reader options */
%token KW_PAD_SIZE KW_TIME_ZONE KW_RECV_TIME_ZONE KW_SEND_TIME_ZONE

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
%token KW_REMOVE_IF_OLDER

/* socket related options */
%token KW_KEEP_ALIVE KW_MAX_CONNECTIONS
%token KW_LOCALIP KW_IP KW_LOCALPORT KW_PORT KW_DESTPORT 
%token KW_IP_TTL KW_SO_BROADCAST KW_IP_TOS KW_SO_SNDBUF KW_SO_RCVBUF

/* misc options */
%token KW_USE_TIME_RECVD

/* filter items*/
%token KW_FACILITY KW_LEVEL KW_HOST KW_MATCH KW_NETMASK

/* yes/no switches */
%token KW_YES KW_NO

/* tripleoption */
%token KW_REQUIRED KW_ALLOW KW_DENY

/* obsolete, compatibility and not-yet supported options */
%token KW_GC_IDLE_THRESHOLD KW_GC_BUSY_THRESHOLD  
%token KW_COMPRESS KW_MAC KW_AUTH KW_ENCRYPT

%token  DOTDOT
%token	<cptr> IDENTIFIER
%token	<num>  NUMBER
%token	<cptr> STRING

%left	KW_OR
%left	KW_AND
%left   KW_NOT

%type	<ptr> source_stmt
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
%type   <ptr> source_afsocket_stream_params
%type	<ptr> source_afstreams
%type	<ptr> source_afstreams_params
%type	<num> source_reader_option_flags

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
%type   <ptr> dest_afuser
%type   <ptr> dest_afprogram
%type   <ptr> dest_afprogram_params
%type	<num> dest_writer_options_flags
%type	<num> dest_writer_options_flag

%type	<ptr> log_items
%type	<ptr> log_item
%type	<num> log_flags
%type   <num> log_flags_items
%type	<num> log_flags_item

%type	<ptr> options_items
%type	<ptr> options_item

%type	<ptr> filter_stmt
%type	<node> filter_expr
%type	<node> filter_simple_expr

%type   <num> filter_fac_list
%type   <num> filter_fac
%type	<num> filter_level_list
%type   <num> filter_level

%type	<num> yesno
%type   <num> dnsmode

%type	<cptr> string

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
	| KW_TEMPLATE template_stmt		{ cfg_add_template(configuration, $2); }
	| KW_OPTIONS options_stmt		{  }
	;

source_stmt
	: string '{' source_items '}'		{ $$ = log_source_group_new($1, $3); free($1); }        
	;

dest_stmt
        : string '{' dest_items '}'		{ $$ = log_dest_group_new($1, $3); free($1); }
	;

log_stmt
        : '{' log_items log_flags '}'		{ $$ = log_connection_new($2, $3); }
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
	: KW_TEMPLATE '(' string ')'		{ last_template->template = g_string_new($3); free($3); }
	| KW_TEMPLATE_ESCAPE '(' yesno ')'	{ log_template_set_escape(last_template, $3); }
	;

socket_option
	: KW_SO_SNDBUF '(' NUMBER ')'           { last_sock_options->sndbuf = $3; }
	| KW_SO_RCVBUF '(' NUMBER ')'           { last_sock_options->rcvbuf = $3; }
	| KW_SO_BROADCAST '(' yesno ')'         { last_sock_options->broadcast = $3; }
	;

inet_socket_option
	: socket_option
	| KW_IP_TTL '(' NUMBER ')'              { ((InetSocketOptions *) last_sock_options)->ttl = $3; }
	| KW_IP_TOS '(' NUMBER ')'              { ((InetSocketOptions *) last_sock_options)->tos = $3; }
	;

source_items
        : source_item ';' source_items		{ log_drv_append($1, $3); log_drv_unref($3); $$ = $1; }
	|					{ $$ = NULL; }
	;

source_item
  	: source_afinter			{ $$ = $1; }
	| source_affile				{ $$ = $1; }
	| source_afsocket			{ $$ = $1; }
	| source_afstreams			{ $$ = $1; }
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
	  source_reader_options			{ $$ = last_driver; }
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
	: KW_OWNER '(' string ')'		{ afunix_sd_set_uid(last_driver, $3); free($3); }
	| KW_GROUP '(' string ')'		{ afunix_sd_set_gid(last_driver, $3); free($3); }
	| KW_PERM '(' NUMBER ')'		{ afunix_sd_set_perm(last_driver, $3); }
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
	| KW_LOCALPORT '(' string ')'		{ afinet_sd_set_localport(last_driver, 0, $3, "udp"); free($3); }
	| KW_PORT '(' string ')'		{ afinet_sd_set_localport(last_driver, 0, $3, "udp"); free($3); }
	;

source_afinet_option
	: KW_LOCALIP '(' string ')'		{ afinet_sd_set_localip(last_driver, $3); free($3); }
	| KW_LOCALPORT '(' NUMBER ')'		{ afinet_sd_set_localport(last_driver, $3, NULL, NULL); }
	| KW_PORT '(' NUMBER ')'		{ afinet_sd_set_localport(last_driver, $3, NULL, NULL); }
	| KW_IP '(' string ')'			{ afinet_sd_set_localip(last_driver, $3); free($3); }
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
	| KW_LOCALPORT '(' string ')'		{ afinet_sd_set_localport(last_driver, 0, $3, "tcp"); free($3); }
	| KW_PORT '(' string ')'		{ afinet_sd_set_localport(last_driver, 0, $3, "tcp"); free($3); }
	| source_afsocket_stream_params		{}
	;

source_afsocket_stream_params
	: KW_KEEP_ALIVE '(' yesno ')'		{ afsocket_sd_set_keep_alive(last_driver, $3); }
	| KW_MAX_CONNECTIONS '(' NUMBER ')'	{ afsocket_sd_set_max_connections(last_driver, $3); }
	;
	
source_afstreams
	: KW_SUN_STREAMS '(' source_afstreams_params ')'	{ $$ = $3; }
	;
	
source_afstreams_params
	: string
	  { 
	    last_driver = afstreams_sd_new($1); 
	    free($1); 
	  }
	  source_afstreams_options		{ $$ = last_driver; }
	;
	
source_afstreams_options
	: source_afstreams_option source_afstreams_options
	|
	;

source_afstreams_option
	: KW_DOOR '(' string ')'		{ afstreams_sd_set_sundoor(last_driver, $3); free($3); }
	;
	
source_reader_options
	: source_reader_option source_reader_options
	|
	;
	
source_reader_option
	: KW_FLAGS '(' source_reader_option_flags ')' { last_reader_options->options = $3; }
	| KW_LOG_MSG_SIZE '(' NUMBER ')'	{ last_reader_options->msg_size = $3; }
	| KW_LOG_IW_SIZE '(' NUMBER ')'		{ last_reader_options->source_opts.init_window_size = $3; }
	| KW_LOG_FETCH_LIMIT '(' NUMBER ')'	{ last_reader_options->fetch_limit = $3; }
	| KW_LOG_PREFIX '(' string ')'		{ last_reader_options->prefix = $3; }
	| KW_PAD_SIZE '(' NUMBER ')'		{ last_reader_options->padding = $3; }
	| KW_FOLLOW_FREQ '(' NUMBER ')'		{ last_reader_options->follow_freq = $3; }
	| KW_TIME_ZONE '(' string ')'		{ cfg_timezone_value($3, &last_reader_options->zone_offset); free($3); }
	| KW_KEEP_TIMESTAMP '(' yesno ')'	{ last_reader_options->keep_timestamp = $3; }
	;

source_reader_option_flags
	: IDENTIFIER source_reader_option_flags { $$ = lookup_parse_flag($1) | $2; free($1); }
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
	| KW_OPTIONAL '(' yesno ')'			{ last_driver->optional = $3; }
/*
	| KW_COMPRESS '(' yesno ')'		{ affile_dd_set_compress(last_driver, $3); }
	| KW_ENCRYPT '(' yesno ')'		{ affile_dd_set_encrypt(last_driver, $3); }
*/
	| KW_OWNER '(' string ')'		{ affile_dd_set_file_uid(last_driver, $3); free($3); }
	| KW_GROUP '(' string ')'		{ affile_dd_set_file_gid(last_driver, $3); free($3); }
	| KW_PERM '(' NUMBER ')'		{ affile_dd_set_file_perm(last_driver, $3); }
	| KW_DIR_OWNER '(' string ')'		{ affile_dd_set_dir_uid(last_driver, $3); free($3); }
	| KW_DIR_GROUP '(' string ')'		{ affile_dd_set_dir_gid(last_driver, $3); free($3); }
	| KW_DIR_PERM '(' NUMBER ')'		{ affile_dd_set_dir_perm(last_driver, $3); }
	| KW_CREATE_DIRS '(' yesno ')'		{ affile_dd_set_create_dirs(last_driver, $3); }
	| KW_REMOVE_IF_OLDER '(' NUMBER ')'	{ affile_dd_set_remove_if_older(last_driver, $3); }
	;

dest_afpipe
	: KW_PIPE '(' dest_afpipe_params ')'    { $$ = $3; }
	;

dest_afpipe_params
	: string 
	  { 
	    last_driver = affile_dd_new($1, AFFILE_NO_EXPAND | AFFILE_PIPE);
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
	| KW_OWNER '(' string ')'		{ affile_dd_set_file_uid(last_driver, $3); free($3); }
	| KW_GROUP '(' string ')'		{ affile_dd_set_file_gid(last_driver, $3); free($3); }
	| KW_PERM '(' NUMBER ')'		{ affile_dd_set_file_perm(last_driver, $3); }
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
	| KW_PORT '(' NUMBER ')'		{ afinet_dd_set_destport(last_driver, $3, NULL, NULL); }
	| inet_socket_option
	| dest_writer_option
	;

dest_afinet_udp_option
	: dest_afinet_option
	| KW_LOCALPORT '(' string ')'		{ afinet_dd_set_localport(last_driver, 0, $3, "udp"); free($3); }
	| KW_PORT '(' string ')'		{ afinet_dd_set_destport(last_driver, 0, $3, "udp"); free($3); }
	| KW_DESTPORT '(' string ')'		{ afinet_dd_set_destport(last_driver, 0, $3, "udp"); free($3); }
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
	| KW_LOCALPORT '(' string ')'		{ afinet_dd_set_localport(last_driver, 0, $3, "tcp"); free($3); }
	| KW_PORT '(' string ')'		{ afinet_dd_set_destport(last_driver, 0, $3, "tcp"); free($3); }
	| KW_DESTPORT '(' string ')'		{ afinet_dd_set_destport(last_driver, 0, $3, "tcp"); free($3); }
/*
	| KW_MAC '(' yesno ')'
	| KW_AUTH '(' yesno ')'
	| KW_ENCRYPT '(' yesno ')'
*/
	;


dest_afuser
	: KW_USER '(' string ')'		{ $$ = afuser_dd_new($3); free($3); }
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
	
dest_writer_options
	: dest_writer_option dest_writer_options 
	|
	;
	
dest_writer_option
	: KW_FLAGS '(' dest_writer_options_flags ')' { last_writer_options->options = $3; }
	| KW_LOG_FIFO_SIZE '(' NUMBER ')'	{ last_writer_options->fifo_size = $3; }
	| KW_FLUSH_LINES '(' NUMBER ')'		{ last_writer_options->flush_lines = $3; }
	| KW_FLUSH_TIMEOUT '(' NUMBER ')'	{ last_writer_options->flush_timeout = $3; }
	| KW_TEMPLATE '(' string ')'       	{ last_writer_options->template = cfg_lookup_template(configuration, $3);
	                                          if (last_writer_options->template == NULL)
	                                            {
	                                              last_writer_options->template = log_template_new(NULL, $3); 
	                                              last_writer_options->template->def_inline = TRUE;
	                                            }
	                                          else
	                                            log_template_ref(last_writer_options->template);
	                                          free($3);
	                                        }
	| KW_TEMPLATE_ESCAPE '(' yesno ')'	{ log_writer_options_set_template_escape(last_writer_options, $3); }
	| KW_FSYNC '(' yesno ')'		{ msg_error("fsync() does not work yet", NULL); }
	| KW_TIME_ZONE '(' string ')'           { cfg_timezone_value($3, &last_writer_options->zone_offset); free($3); }
	| KW_TS_FORMAT '(' string ')'		{ last_writer_options->ts_format = cfg_ts_format_value($3); free($3); }
	| KW_FRAC_DIGITS '(' NUMBER ')'		{ last_writer_options->frac_digits = $3; }
	;

dest_writer_options_flags
	: dest_writer_options_flag dest_writer_options_flags { $$ = $1 | $2; }
	|					{ $$ = 0; }
	;

dest_writer_options_flag
	: KW_TMPL_ESCAPE			{ $$ = LWO_TMPL_ESCAPE; }
	;


log_items
	: log_item ';' log_items		{ log_endpoint_append($1, $3); $$ = $1; }
	|					{ $$ = NULL; }
	;

log_item
	: KW_SOURCE '(' string ')'		{ $$ = log_endpoint_new(EP_SOURCE, $3); free($3); }
	| KW_FILTER '(' string ')'		{ $$ = log_endpoint_new(EP_FILTER, $3); free($3); }
	| KW_DESTINATION '(' string ')'		{ $$ = log_endpoint_new(EP_DESTINATION, $3); free($3); }
	;

log_flags
	: KW_FLAGS '(' log_flags_items ')' ';'	{ $$ = $3; }
	|					{ $$ = 0; }
	;


log_flags_items
	: log_flags_item log_flags_items	{ $$ |= $2; }
	|					{ $$ = 0; }
	;

log_flags_item
	: KW_CATCHALL				{ $$ = LC_CATCHALL; }
	| KW_FALLBACK				{ $$ = LC_FALLBACK; }
	| KW_FINAL				{ $$ = LC_FINAL; }
	| KW_FLOW_CONTROL			{ $$ = LC_FLOW_CONTROL; }
	;

options_items
	: options_item ';' options_items	{ $$ = $1; }
	|					{ $$ = NULL; }
	;

options_item
	: KW_MARK_FREQ '(' NUMBER ')'		{ configuration->mark_freq = $3; }
	| KW_STATS_FREQ '(' NUMBER ')'          { configuration->stats_freq = $3; }
	| KW_FLUSH_LINES '(' NUMBER ')'		{ configuration->flush_lines = $3; }
	| KW_FLUSH_TIMEOUT '(' NUMBER ')'	{ configuration->flush_timeout = $3; }
	| KW_CHAIN_HOSTNAMES '(' yesno ')'	{ configuration->chain_hostnames = $3; }
	| KW_NORMALIZE_HOSTNAMES '(' yesno ')'	{ configuration->normalize_hostnames = $3; }
	| KW_KEEP_HOSTNAME '(' yesno ')'	{ configuration->keep_hostname = $3; }
	| KW_CHECK_HOSTNAME '(' yesno ')'	{ configuration->check_hostname = $3; }
	| KW_BAD_HOSTNAME '(' STRING ')'	{ cfg_bad_hostname_set(configuration, $3); free($3); }
	| KW_USE_TIME_RECVD '(' yesno ')'	{ configuration->use_time_recvd = $3; }
	| KW_USE_FQDN '(' yesno ')'		{ configuration->use_fqdn = $3; }
	| KW_USE_DNS '(' dnsmode ')'		{ configuration->use_dns = $3; }
	| KW_TIME_REOPEN '(' NUMBER ')'		{ configuration->time_reopen = $3; }
	| KW_TIME_REAP '(' NUMBER ')'		{ configuration->time_reap = $3; }
	| KW_TIME_SLEEP '(' NUMBER ')'		
		{ 
		  configuration->time_sleep = $3; 
		  if ($3 > 500) 
		    { 
		      msg_notice("The value specified for time_sleep is too large", evt_tag_int("time_sleep", $3), NULL);
		      configuration->time_sleep = 500;
		    }
		}
	| KW_LOG_FIFO_SIZE '(' NUMBER ')'	{ configuration->log_fifo_size = $3; }
	| KW_LOG_IW_SIZE '(' NUMBER ')'		{ configuration->log_iw_size = $3; }
	| KW_LOG_FETCH_LIMIT '(' NUMBER ')'	{ configuration->log_fetch_limit = $3; }
	| KW_LOG_MSG_SIZE '(' NUMBER ')'	{ configuration->log_msg_size = $3; }
	| KW_KEEP_TIMESTAMP '(' yesno ')'	{ configuration->keep_timestamp = $3; }
	| KW_TS_FORMAT '(' string ')'		{ configuration->ts_format = cfg_ts_format_value($3); free($3); }
	| KW_FRAC_DIGITS '(' NUMBER ')'		{ configuration->frac_digits = $3; }
	| KW_GC_BUSY_THRESHOLD '(' NUMBER ')' 	{ /* ignored */; }
	| KW_GC_IDLE_THRESHOLD '(' NUMBER ')'	{ /* ignored */; }
	| KW_CREATE_DIRS '(' yesno ')'		{ configuration->create_dirs = $3; }
	| KW_OWNER '(' string ')'		{ cfg_file_owner_set(configuration, $3); free($3); }
	| KW_GROUP '(' string ')'		{ cfg_file_group_set(configuration, $3); free($3); }
	| KW_PERM '(' NUMBER ')'		{ cfg_file_perm_set(configuration, $3); }
	| KW_DIR_OWNER '(' string ')'		{ cfg_dir_owner_set(configuration, $3); free($3); }
	| KW_DIR_GROUP '(' string ')'		{ cfg_dir_group_set(configuration, $3); free($3); }
	| KW_DIR_PERM '(' NUMBER ')'		{ cfg_dir_perm_set(configuration, $3); }
	| KW_DNS_CACHE '(' yesno ')' 		{ configuration->use_dns_cache = $3; }
	| KW_DNS_CACHE_SIZE '(' NUMBER ')'	{ configuration->dns_cache_size = $3; }
	| KW_DNS_CACHE_EXPIRE '(' NUMBER ')'	{ configuration->dns_cache_expire = $3; }
	| KW_DNS_CACHE_EXPIRE_FAILED '(' NUMBER ')'
	  			{ configuration->dns_cache_expire_failed = $3; }
	| KW_DNS_CACHE_HOSTS '(' string ')'     { configuration->dns_cache_hosts = $3; }
	| KW_FILE_TEMPLATE '(' string ')'	{ configuration->file_template_name = $3; }
	| KW_PROTO_TEMPLATE '(' string ')'	{ configuration->proto_template_name = $3; }
	| KW_RECV_TIME_ZONE '(' string ')'      { cfg_timezone_value($3, &configuration->recv_zone_offset); free($3); }
	| KW_SEND_TIME_ZONE '(' string ')'      { cfg_timezone_value($3, &configuration->send_zone_offset); free($3); }
	;

filter_stmt
	: string '{' filter_expr ';' '}'	{ $$ = log_filter_rule_new($1, $3); free($1); }
	;

filter_expr
	: filter_simple_expr			{ $$ = $1; if (!$1) return 1; }
        | KW_NOT filter_expr			{ $2->comp = !($2->comp); $$ = $2; }
	| filter_expr KW_OR filter_expr		{ $$ = fop_or_new($1, $3); }
	| filter_expr KW_AND filter_expr	{ $$ = fop_and_new($1, $3); }
	| '(' filter_expr ')'			{ $$ = $2; }
	;

filter_simple_expr
	: KW_FACILITY '(' filter_fac_list ')'	{ $$ = filter_facility_new($3);  }
	| KW_FACILITY '(' NUMBER ')'		{ $$ = filter_facility_new(0x80000000 | $3); }
	| KW_LEVEL '(' filter_level_list ')' 	{ $$ = filter_level_new($3); }
	| KW_PROGRAM '(' string ')'		{ $$ = filter_prog_new($3); free($3); }
	| KW_HOST '(' string ')'		{ $$ = filter_host_new($3); free($3); }	
	| KW_MATCH '(' string ')'		{ $$ = filter_match_new($3); free($3); }
	| KW_FILTER '(' string ')'		{ $$ = filter_call_new($3, configuration); free($3); }
	| KW_NETMASK '(' string ')'		{ $$ = filter_netmask_new($3); free($3); }
	;

filter_fac_list
	: filter_fac filter_fac_list		{ $$ = $1 + $2; }
	| filter_fac				{ $$ = $1; }
	;

filter_fac
	: IDENTIFIER				
	  { 
	    int n = syslog_name_lookup_facility_by_name($1);
	    if (n == -1)
	      {
	        msg_error("Warning: Unknown facility", 
	                  evt_tag_str("facility", $1),
	                  NULL);
	        $$ = 0;
	      }
	    else
	      $$ = (1 << n); 
	    free($1); 
	  }
	;

filter_level_list
	: filter_level filter_level_list	{ $$ = $1 + $2; }
	| filter_level				{ $$ = $1; }
	;

filter_level
	: IDENTIFIER DOTDOT IDENTIFIER		
	  { 
	    int r1, r2;
	    r1 = syslog_name_lookup_level_by_name($1);
	    if (r1 == -1)
	      msg_error("Warning: Unknown priority level",
                        evt_tag_str("priority", $1),
                        NULL);
	    r2 = syslog_name_lookup_level_by_name($3);
	    if (r2 == -1)
	      msg_error("Warning: Unknown priority level",
                        evt_tag_str("priority", $1),
                        NULL);
	    if (r1 != -1 && r2 != -1)
	      $$ = syslog_make_range(r1, r2); 
	    else
	      $$ = 0;
	    free($1); 
	    free($3); 
	  }
	| IDENTIFIER				
	  { 
	    int n = syslog_name_lookup_level_by_name($1); 
	    if (n == -1)
	      {
	        msg_error("Warning: Unknown priority level",
                          evt_tag_str("priority", $1),
                          NULL);
	        $$ = 0;
	      }
	    else
	      $$ = 1 << n;
	    free($1); 
	  }
	;

yesno
	: KW_YES				{ $$ = 1; }
	| KW_NO					{ $$ = 0; }
	| NUMBER				{ $$ = $1; }
	;

dnsmode
	: yesno					{ $$ = $1; }
	| KW_PERSIST_ONLY                       { $$ = 2; }
	;

string
	: IDENTIFIER
	| STRING
	;

%%

extern int linenum;

void 
yyerror(char *msg)
{
  fprintf(stderr, "%s at %d\n", msg, linenum);
}

void
yyparser_reset(void)
{
  last_driver = NULL;
  last_reader_options = NULL;
  last_writer_options = NULL;
  last_template = NULL;
}