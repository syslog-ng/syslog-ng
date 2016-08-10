from func_tests.testcase import Testcase

port_number = 1111
src_dir = "ose_sandbox/"
ssl_port_number = 1112
port_number_syslog = 1113
port_number_network = 1114

config = """@version: 3.8

options { ts_format(iso); chain_hostnames(no); keep_hostname(yes); threaded(yes); };

source s_int { internal(); };
source s_unix { unix-stream("log-stream" flags(expect-hostname)); unix-dgram("log-dgram" flags(expect-hostname));  };
source s_inet { tcp(port(%(port_number)d)); udp(port(%(port_number)d) so_rcvbuf(131072)); };
source s_inetssl { tcp(port(%(ssl_port_number)d) tls(peer-verify(none) cert-file("%(src_dir)s/ssl.crt") key-file("%(src_dir)s/ssl.key"))); };
source s_pipe { pipe("log-pipe" flags(expect-hostname)); pipe("log-padded-pipe" pad_size(2048) flags(expect-hostname)); };
source s_file { file("log-file"); };
source s_network { network(transport(udp) port(%(port_number_network)s)); network(transport(tcp) port(%(port_number_network)s)); };
source s_catchall { unix-stream("log-stream-catchall" flags(expect-hostname)); };

source s_syslog { syslog(port(%(port_number_syslog)d) transport("tcp") so_rcvbuf(131072)); syslog(port(%(port_number_syslog)d) transport("udp") so_rcvbuf(131072)); };

# test input drivers
filter f_input1 { message("input_drivers"); };
destination d_input1 { file("test-input1.log"); };
destination d_input1_new { file("test-input1_new.log" flags(syslog-protocol)); };

log { source(s_int); source(s_unix); source(s_inet); source(s_inetssl); source(s_pipe); source(s_file); source(s_network);
        log { filter(f_input1); destination(d_input1); };
};

log { source(s_syslog);
        log { filter(f_input1); destination(d_input1_new); };
};

destination d_indep1 { file("test-indep1.log"); };
destination d_indep2 { file("test-indep2.log"); };

filter f_indep { message("indep_pipes"); };

# test independent log pipes
log { source(s_int); source(s_unix); source(s_inet); source(s_inetssl); filter(f_indep);
        log { destination(d_indep1); };
        log { destination(d_indep2); };
};

filter f_final { message("final"); };
filter f_final_1_to_4 { message("final[1-4]"); };

filter f_final1 { message("final1"); };
filter f_final2 { message("final2"); };
filter f_final3 { message("final3"); };

destination d_final1 { file("test-final1.log"); };
destination d_final2 { file("test-final2.log"); };
destination d_final3 { file("test-final3.log"); };
destination d_final4 { file("test-final4.log"); };
destination d_final5 { file("test-final5.log"); };


# test final flag + rest
log { source(s_int); source(s_unix); source(s_inet); source(s_inetssl); filter(f_final);

        filter(f_final_1_to_4);
        log { filter(f_final1); destination(d_final1); flags(final); };
        log { filter(f_final2); destination(d_final2); flags(final); };
        log { filter(f_final3); destination(d_final3); flags(final); };
        log { destination(d_final4); };
        flags(final);
};

# fallback, top-level log path for final flags. No messages should be
# delivered from final1-4, because of the final flag above.  final5 messages
# will get here, as the filter above excludes that from the previous log
# statement.

log { source(s_int); source(s_unix); source(s_inet); source(s_inetssl);

        # this filter would match everything from final1 to final5, but the
        # flags(final) in the previous statement would stop the processing
        # for final1-4

        filter(f_final);
        destination(d_final5);
};

filter f_fb { message("fallback"); };
filter f_fb1 { message("fallback1"); };
filter f_fb2 { message("fallback2"); };
filter f_fb3 { message("fallback3"); };

destination d_fb1 { file("test-fb1.log"); };
destination d_fb2 { file("test-fb2.log"); };
destination d_fb3 { file("test-fb3.log"); };
destination d_fb4 { file("test-fb4.log"); };

# test fallback flag
log { source(s_int); source(s_unix); source(s_inet); source(s_inetssl); filter(f_fb);
        log { destination(d_fb4); flags(fallback); };
        log { filter(f_fb1); destination(d_fb1); };
        log { filter(f_fb2); destination(d_fb2); };
        log { filter(f_fb3); destination(d_fb3); };
};

# test catch-all flag
filter f_catchall { message("catchall"); };

destination d_catchall { file("test-catchall.log"); };

log { filter(f_catchall); destination(d_catchall); flags(catch-all); };
""" % locals()

def test_valami():
    tc = Testcase()
    tc.config.from_raw(config)
    tc.syslog_ng.start()
    import time
    time.sleep(1)
    tc.syslog_ng.stop()
