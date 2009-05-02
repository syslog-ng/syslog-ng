from globals import *
from log import *
from messagegen import *
from messagecheck import *

config = """@version: 3.0

options { ts_format(iso); chain_hostnames(no); keep_hostname(yes); };

source s_int { internal(); };
source s_unix { unix-stream("log-stream"); unix-dgram("log-dgram");  };
source s_inet { tcp(port(%(port_number)d)); udp(port(%(port_number)d) so_rcvbuf(131072)); };
source s_inetssl { tcp(port(%(ssl_port_number)d) tls(peer-verify(none) cert-file("%(src_dir)s/ssl.crt") key-file("%(src_dir)s/ssl.key"))); };
source s_pipe { pipe("log-pipe"); pipe("log-padded-pipe" pad_size(2048)); };
source s_file { file("log-file"); };
source s_catchall { unix-stream("log-stream-catchall"); };

filter f_input1 { message("input_drivers"); };
destination d_input1 { file("test-input1.log"); logstore("test-input1.lgs"); };

log { source(s_int); source(s_unix); source(s_inet); source(s_inetssl); source(s_pipe); source(s_file);
        log { filter(f_input1); destination(d_input1); };
};


destination d_indep1 { file("test-indep1.log"); logstore("test-indep1.lgs"); };
destination d_indep2 { file("test-indep2.log"); logstore("test-indep2.lgs"); };

filter f_indep { message("indep_pipes"); };

# test independent log pipes
log { source(s_int); source(s_unix); source(s_inet); source(s_inetssl); filter(f_indep);
        log { destination(d_indep1); };
        log { destination(d_indep2); };
};

filter f_final { message("final"); };

filter f_final1 { message("final1"); };
filter f_final2 { message("final2"); };
filter f_final3 { message("final3"); };

destination d_final1 { file("test-final1.log"); logstore("test-final1.lgs"); };
destination d_final2 { file("test-final2.log"); logstore("test-final2.lgs"); };
destination d_final3 { file("test-final3.log"); logstore("test-final3.lgs"); };
destination d_final4 { file("test-final4.log"); logstore("test-final4.lgs"); };


# test final flag + rest
log { source(s_int); source(s_unix); source(s_inet); source(s_inetssl);  filter(f_final);
        log { filter(f_final1); destination(d_final1); flags(final); };
        log { filter(f_final2); destination(d_final2); flags(final); };
        log { filter(f_final3); destination(d_final3); flags(final); };
        log { destination(d_final4); };
};

filter f_fb { message("fallback"); };
filter f_fb1 { message("fallback1"); };
filter f_fb2 { message("fallback2"); };
filter f_fb3 { message("fallback3"); };

destination d_fb1 { file("test-fb1.log"); logstore("test-fb1.lgs"); };
destination d_fb2 { file("test-fb2.log"); logstore("test-fb2.lgs"); };
destination d_fb3 { file("test-fb3.log"); logstore("test-fb3.lgs"); };
destination d_fb4 { file("test-fb4.log"); logstore("test-fb4.lgs"); };

# test fallback flag
log { source(s_int); source(s_unix); source(s_inet); source(s_inetssl); filter(f_fb);
        log { destination(d_fb4); flags(fallback); };
        log { filter(f_fb1); destination(d_fb1); };
        log { filter(f_fb2); destination(d_fb2); };
        log { filter(f_fb3); destination(d_fb3); };
};

# test catch-all flag
filter f_catchall { message("catchall"); };

destination d_catchall { file("test-catchall.log"); logstore("test-catchall.lgs"); };

log { filter(f_catchall); destination(d_catchall); flags(catch-all); };
""" % locals()


def test_input_drivers():
    message = 'input_drivers';

    senders = (
        SocketSender(AF_UNIX, 'log-dgram', dgram=1),
        SocketSender(AF_UNIX, 'log-dgram', dgram=1, terminate_seq='\0'),
        SocketSender(AF_UNIX, 'log-dgram', dgram=1, terminate_seq='\0\n'),
        SocketSender(AF_UNIX, 'log-stream', dgram=0),
        SocketSender(AF_UNIX, 'log-stream', dgram=0, send_by_bytes=1),
        SocketSender(AF_INET, ('localhost', port_number), dgram=1),
        SocketSender(AF_INET, ('localhost', port_number), dgram=1, terminate_seq='\0'),
        SocketSender(AF_INET, ('localhost', port_number), dgram=1, terminate_seq='\0\n'),
        SocketSender(AF_INET, ('localhost', port_number), dgram=0),
        SocketSender(AF_INET, ('localhost', port_number), dgram=0, send_by_bytes=1),
        SocketSender(AF_INET, ('localhost', ssl_port_number), dgram=0, ssl=1),
        SocketSender(AF_INET, ('localhost', ssl_port_number), dgram=0, send_by_bytes=1, ssl=1),
        FileSender('log-pipe'),
        FileSender('log-pipe', send_by_bytes=1),
        FileSender('log-padded-pipe', padding=2048),
        FileSender('log-padded-pipe', padding=2048),
        FileSender('log-file'),
        FileSender('log-file', send_by_bytes=1),
    )

    expected = []
    for s in senders:
        expected.extend(s.sendMessages(message))

    return check_file_expected("test-input1", expected, settle_time=6);


def test_indep():
    message = 'indep_pipes';

    s = SocketSender(AF_UNIX, 'log-stream', dgram=0, repeat=10)
    expected = s.sendMessages(message)
    return check_file_expected("test-indep1", expected) and check_file_expected("test-indep2", expected)

def test_final():
    messages = (
      'final1',
      'final2',
      'final3',
      'final4'
    )
    expected = [None,] * len(messages)

    s = SocketSender(AF_UNIX, 'log-stream', dgram=0, repeat=10)
    for ndx in range(0, len(messages)):
        if not expected[ndx]:
            expected[ndx] = []
        expected[ndx].extend(s.sendMessages(messages[ndx]))

    for ndx in range(0, len(messages)):
        if not check_file_expected('test-final%d' % (ndx + 1,), expected[ndx]):
            return False
    return True

def test_fallback():
    messages = (
      'fallback1',
      'fallback2',
      'fallback3',
      'fallback4',
    )
    expected = [None,] * len(messages)

    s = SocketSender(AF_UNIX, 'log-stream', dgram=0, repeat=10)
    for ndx in range(0, len(messages)):
        if not expected[ndx]:
            expected[ndx] = []
        expected[ndx].extend(s.sendMessages(messages[ndx]))

    for ndx in range(0, len(messages)):
        if not check_file_expected('test-fb%d' % (ndx + 1,), expected[ndx]):
            return False
    return True

def test_catchall():

    message = 'catchall';

    senders = (
        SocketSender(AF_UNIX, 'log-stream-catchall', dgram=0, send_by_bytes=1),
        SocketSender(AF_UNIX, 'log-dgram', dgram=1),
        SocketSender(AF_UNIX, 'log-dgram', dgram=1, terminate_seq='\0'),
        SocketSender(AF_UNIX, 'log-dgram', dgram=1, terminate_seq='\0\n'),
        SocketSender(AF_UNIX, 'log-stream', dgram=0),
        SocketSender(AF_UNIX, 'log-stream', dgram=0, send_by_bytes=1),
        SocketSender(AF_INET, ('localhost', port_number), dgram=1),
        SocketSender(AF_INET, ('localhost', port_number), dgram=1, terminate_seq='\0'),
        SocketSender(AF_INET, ('localhost', port_number), dgram=1, terminate_seq='\0\n'),
        SocketSender(AF_INET, ('localhost', port_number), dgram=0),
        SocketSender(AF_INET, ('localhost', port_number), dgram=0, send_by_bytes=1),
        FileSender('log-pipe'),
        FileSender('log-pipe', send_by_bytes=1),
        FileSender('log-padded-pipe', padding=2048),
        FileSender('log-padded-pipe', padding=2048),
    )

    expected = []
    for s in senders:
        expected.extend(s.sendMessages(message))

    return check_file_expected("test-catchall", expected);

