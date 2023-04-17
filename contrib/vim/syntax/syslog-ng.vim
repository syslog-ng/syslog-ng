" Vim syntax file
" Language:	syslog-ng
" Maintainer:	syslog-ng contributors
" Updaters:	Refer to `git log`
" URL:		https://github.com/syslog-ng/syslog-ng/tree/master/contrib/vim
" Changes:	Refer to `git log`
" Last Change:	2023-04-12

" Remove any old syntax stuff hanging around
syn clear
syn case match
set iskeyword=a-z,A-Z,48-57,_,-

syn keyword sysngObject log
syn keyword sysngObject if else elif
" We cannot use keyword here because of higher priority
syn match sysngObject "\(destination\|options\|parser\|filter\|rewrite\|source\)"

syn match sysngParameter "\(destination\|options\|parser\|filter\|rewrite\|source\)\ze[[:blank:]]*("

" This list was generated with:
" awk 'BEGIN { kw=""; reserved["options"] = reserved["and"] = reserved["or"] = reserved["log"] = reserved["source"] = reserved["filter"] = reserved["parser"] = reserved["rewrite"] = reserved["destination"] = reserved["yes"] = reserved["on"] = reserved["no"] = reserved["off"] = reserved["auto"] = reserved["if"] = reserved["elif"] = reserved["else"] = 1 } $1 == "{" && $2 ~ /^".*",$/ && $3 ~ /^KW_/ { gsub(/",?/, "", $2); if (!($2 in reserved)) { kw = kw" "$2 } } END { print kw }' lib/cfg-parser.c lib/*/*parser.c modules/*/*parser.c | tr ' ' '\n' | sed '/_/ { p; s/_/-/g }' | sort| uniq  | tr '\n' ' '
syn keyword sysngParameter accept-redirects accept_redirects ack add-prefix add_prefix address aggregate allow-compress allow_compress amqp application attributes auth auth-algorithm auth_algorithm auth-method auth_method auth-password auth_password auth-username auth_username azure-auth-header azure_auth_header bad-hostname bad_hostname base-dir base_dir batch-bytes batch_bytes batch-lines batch_lines batch-timeout batch_timeout bcc block body body-prefix body_prefix body-suffix body_suffix bootstrap-servers bootstrap_servers cacert ca-dir ca_dir ca-file ca_file cast cc cert cert-file cert_file chain-hostnames chain_hostnames channel chars check-hostname check_hostname cipher-suite cipher_suite class cleansession clear-tag clear_tag client-id client_id client-sigalgs client_sigalgs close-on-input close_on_input collection columns command community compaction condition config content-type content_type create-dirs create_dirs create-lists create_lists create-statement-append create_statement_append crl-dir crl_dir csv-parser csv_parser curve-list curve_list custom-domain custom_domain database date-parser date_parser dbd-option dbd_option db-parser db_parser default default-facility default_facility default-level default_level default-priority default_priority default-severity default_severity delimiter delimiters description destport dhparam-file dhparam_file dialect dir dir-group dir_group dir-owner dir_owner dir-perm dir_perm disconnect disk-buffer disk_buffer disk-buf-size disk_buf_size dns-cache dns_cache dns-cache-expire dns_cache_expire dns-cache-expire-failed dns_cache_expire_failed dns-cache-hosts dns_cache_hosts dns-cache-size dns_cache_size door drop drop-invalid drop_invalid drop-unmatched drop_unmatched dynamic-window-realloc-ticks dynamic_window_realloc_ticks dynamic-window-size dynamic_window_size dynamic-window-stats-freq dynamic_window_stats_freq ecdh-curve-list ecdh_curve_list enc-algorithm enc_algorithm encoding enc-password enc_password engine-id engine_id eq event-time event_time exchange exchange-declare exchange_declare exchange-type exchange_type exclude exclude-tags exclude_tags extract-prefix extract_prefix extract-stray-words-into extract_stray_words_into facility failback failover failover-servers failover_servers fallback-topic fallback_topic fetch-no-data-delay fetch_no_data_delay fifo file filename-pattern filename_pattern file-template file_template fix-time-zone fix_time_zone flags flush-bytes flush_bytes flush-lines flush_lines flush-timeout flush_timeout flush-timeout-on-reload flush_timeout_on_reload flush-timeout-on-shutdown flush_timeout_on_shutdown follow-freq follow_freq force-directory-polling force_directory_polling format frac-digits frac_digits frame-size frame_size freq from fsync ge geoip2 group grouping-by grouping_by groupset groupunset gt guess-time-zone guess_time_zone having header headers heartbeat hook-commands hook_commands host host-override host_override http http-proxy http_proxy ignore-tns-config ignore_tns_config include indexes inherit-environment inherit_environment inherit-mode inherit_mode inject-mode inject_mode in-list in_list interface internal ip ip-freebind ip_freebind ip-protocol ip_protocol ip-tos ip_tos ip-ttl ip_ttl json-parser json_parser junction kafka-c kafka_c keep-alive keep_alive keepalive keep-hostname keep_hostname keep-timestamp keep_timestamp key key-delimiter key_delimiter key-file key_file keylog-file keylog_file kv-parser kv_parser labels le level lifetime linux-audit-parser linux_audit_parser listen-backlog listen_backlog loaders local-creds local_creds localip localport local-time-zone local_time_zone log-fetch-limit log_fetch_limit log-fifo-size log_fifo_size log-iw-size log_iw_size log-level log_level log-msg-size log_msg_size log-prefix log_prefix long-hostnames long_hostnames lt map-value-pairs map_value_pairs mark marker mark-errors-as-critical mark_errors_as_critical mark-freq mark_freq mark-mode mark_mode match match-boot match_boot matches max-channel max_channel max-connections max_connections max-dynamics max_dynamics max-field-size max_field_size max-files max_files mem-buf-length mem_buf_length mem-buf-size mem_buf_size message message-template message_template method metric metrics-probe metrics_probe microseconds min-iw-size-per-reader min_iw_size_per_reader mongodb monitor-method monitor_method mqtt multi-line-garbage multi_line_garbage multi-line-mode multi_line_mode multi-line-prefix multi_line_prefix multi-line-suffix multi_line_suffix multi-line-timeout multi_line_timeout namespace ne netmask netmask6 network normalize-hostnames normalize_hostnames not null ocsp-stapling-verify ocsp_stapling_verify on-error on_error openbsd openssl-conf-cmds openssl_conf_cmds option optional overwrite-if-older overwrite_if_older owner pad-size pad_size pair pair-separator pair_separator password path peer-verify peer_verify perm persistent persist-name persist_name persist-only persist_only pipe pkcs12-file pkcs12_file poll-timeout poll_timeout port prealloc prefix priority program program-override program_override program-template program_template proto-template proto_template proxy pseudofile python python-fetcher python_fetcher python-http-header python_http_header qos qout-size qout_size quote-pairs quote_pairs quotes rate rate-limit rate_limit read-old-records read_old_records recursive recv-time-zone recv_time_zone redis rekey reliable remove-if-older remove_if_older rename replace replace-prefix replace_prefix reply-to reply_to response-action response_action retries retry retry-sql-inserts retry_sql_inserts riemann routing-key routing_key scope sdata-parser sdata_parser sdata-prefix sdata_prefix seconds secret sender send-time-zone send_time_zone server servers service session-statements session_statements set set-facility set_facility set-matches set_matches set-message-macro set_message_macro set-pri set_pri set-severity set_severity set-tag set_tag set-time-zone set_time_zone setup severity shift shift-levels shift_levels shutdown sigalgs smtp sni snmp snmp-obj snmp_obj snmptrapd-parser snmptrapd_parser so-broadcast so_broadcast so-keepalive so_keepalive so-passcred so_passcred so-rcvbuf so_rcvbuf so-reuseport so_reuseport sort-key sort_key so-sndbuf so_sndbuf spoof-source spoof_source spoof-source-max-msglen spoof_source_max_msglen sql ssl-options ssl_options ssl-version ssl_version startup state stats stats-freq stats_freq stats-level stats_level stats-lifetime stats_lifetime stats-max-dynamics stats_max_dynamics stdin stomp strings strip-whitespaces strip_whitespaces subject subst success successful-probes-required successful_probes_required sun-stream sun_stream sun-streams sun_streams suppress symlink-as symlink_as sync sync-freq sync_freq sync-send sync_send syslog syslog-parser syslog_parser syslog-stats syslog_stats systemd-journal systemd_journal systemd-syslog systemd_syslog table tags tags-parser tags_parser tcp tcp6 tcp-keep-alive tcp-keepalive tcp_keep_alive tcp_keepalive tcp-keepalive-intvl tcp_keepalive_intvl tcp-keepalive-probes tcp_keepalive_probes tcp-keepalive-time tcp_keepalive_time tcp-probe-interval tcp_probe_interval teardown template template-escape template_escape template-function template_function threaded throttle timeout time-reap time_reap time-reopen time_reopen time-sleep time_sleep time-stamp time_stamp time-zone time_zone tls tls12-and-older tls12_and_older tls13 to topic transport trap-obj trap_obj trigger trim-large-messages trim_large_messages truncate-size truncate_size truncate-size-ratio truncate_size_ratio trusted-dn trusted_dn trusted-keys trusted_keys ts-format ts_format ttl type udp udp6 unix-dgram unix_dgram unix-stream unix_stream unset unset-matches unset_matches uri url use-dns use_dns use-fqdn use_fqdn user user-agent user_agent use-rcptid use_rcptid username usertty use-syslogng-pid use_syslogng_pid use-system-cert-store use_system_cert_store use-time-recvd use_time_recvd use-uniqid use_uniqid value value-pairs value_pairs values value-separator value_separator version vhost where wildcard-file wildcard_file workers workspace-id workspace_id xml

syn keyword sysngBool		yes on no off auto
syn keyword sysngOperator	and or

syn keyword sysngParameter	remote-control remote_control system

syn keyword sysngIdentifier	escape-double-char escape_double_char escape-none escape_none flow-control flow_control no-parse no_parse nv-pairs nv_pairs pcre regexp store-matches store_matches string strip-whitespace strip_whitespace substring

" Priority
syn keyword sysngIdentifier	emerg alert crit err warning notice info debug
" Deprecaty Priority
syn keyword sysngIdentifier	panic error warn
" Facilities
syn keyword sysngIdentifier	kern user mail daemon auth syslog lpr news uucp cron authpriv ftp ntp security console solaris-cron local0 local1 local2 local3 local4 local5 local6 local7

syn match sysngComment		"#.*$"

" String
syn region sysngString start=+"+ end=+"+ skip=+\\"+ contains=sysngVariableInterpolation
syn region sysngString start=+'+ end=+'+ skip=+\\'+
syn region sysngString start=+`+ end=+`+
syn region sysngVariableInterpolation start="${" end="}" contained

" Numbers
syn match sysngOctNumber	"\<0\o\+\>"
syn match sysngDecNumber	"\<[0-9]\>"
syn match sysngDecNumber	"\<[1-9]\d\+\>"
syn match sysngHexNumber	"\<0x\x\+\>"
syn match sysngIdentifier	"\<[dfprs]_[[:alnum:]_-]\+\>"
syn match sysngObject		"@version: *\d\+\.\d\+"
syn match sysngObject		"@include"
syn match sysngObject		"@define"
syn match sysngObject		"@module"
syn match sysngObject		"@requires"

if !exists("did_sysng_syntax_inits")
    let did_sysng_syntax_inits = 1

    hi link sysngObject			Statement
    hi link sysngComment		Comment
    hi link sysngString			String
    hi link sysngOctNumber		Number
    hi link sysngDecNumber		Number
    hi link sysngHexNumber		Number
    hi link sysngBool			Constant
    hi link sysngIdentifier		Identifier
    hi link sysngVariableInterpolation	Identifier

    hi link sysngParameter		Type
    hi link sysngOperator		Operator
endif

let b:current_syntax = "syslog-ng"
