#############################################################################
# Copyright (c) 2007-2015 Balabit
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as published
# by the Free Software Foundation, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
# As an additional exemption you are allowed to compile & link against the
# OpenSSL libraries as published by the OpenSSL project. See the file
# COPYING for details.
#
#############################################################################

from globals import *
from log import *
from messagegen import *
from messagecheck import *

config = """@version: 3.20

options { ts_format(iso); chain_hostnames(no); keep_hostname(yes); threaded(yes); };

source s_int { internal(); };
source s_unix { unix-stream("log-stream" flags(expect-hostname) listen-backlog(64)); unix-dgram("log-dgram" flags(expect-hostname));  };
source s_inet { tcp(port(%(port_number)d) listen-backlog(64)); udp(port(%(port_number)d) so_rcvbuf(131072)); };
source s_inetssl { tcp(port(%(ssl_port_number)d) tls(peer-verify(none) cert-file("%(src_dir)s/ssl.crt") key-file("%(src_dir)s/ssl.key"))); };
source s_pipe { pipe("log-pipe" flags(expect-hostname)); pipe("log-padded-pipe" pad_size(2048) flags(expect-hostname)); };
source s_file { file("log-file"); };
source s_network { network(transport(udp) port(%(port_number_network)s)); network(transport(tcp) listen-backlog(64) port(%(port_number_network)s)); };
source s_catchall { unix-stream("log-stream-catchall" flags(expect-hostname)); };

source s_syslog { syslog(port(%(port_number_syslog)d) transport("tcp") so_rcvbuf(131072)); syslog(port(%(port_number_syslog)d) transport("udp") so_rcvbuf(131072)); };

# test input drivers
filter f_input1 { message("input_drivers"); };
destination d_input1 { file("test-input1.log"); logstore("test-input1.lgs"); };
destination d_input1_new { file("test-input1_new.log" flags(syslog-protocol)); logstore("test-input1_new.lgs"); };

log { source(s_int); source(s_unix); source(s_inet); source(s_inetssl); source(s_pipe); source(s_file); source(s_network);
        log { filter(f_input1); destination(d_input1); };
};

log { source(s_syslog);
        log { filter(f_input1); destination(d_input1_new); };
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
filter f_final_1_to_4 { message("final[1-4]"); };

filter f_final1 { message("final1"); };
filter f_final2 { message("final2"); };
filter f_final3 { message("final3"); };

destination d_final1 { file("test-final1.log"); logstore("test-final1.lgs"); };
destination d_final2 { file("test-final2.log"); logstore("test-final2.lgs"); };
destination d_final3 { file("test-final3.log"); logstore("test-final3.lgs"); };
destination d_final4 { file("test-final4.log"); logstore("test-final4.lgs"); };
destination d_final5 { file("test-final5.log"); logstore("test-final5.lgs"); };


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
    message_new = 'input_drivers_new';

    senders = (
        SocketSender(AF_UNIX, 'log-dgram', dgram=1, terminate_seq='\n'),
        SocketSender(AF_UNIX, 'log-dgram', dgram=1, terminate_seq='\0'),
        SocketSender(AF_UNIX, 'log-dgram', dgram=1, terminate_seq='\0\n'),
        SocketSender(AF_UNIX, 'log-dgram', dgram=1, terminate_seq=''),
        SocketSender(AF_UNIX, 'log-stream', dgram=0),
        SocketSender(AF_UNIX, 'log-stream', dgram=0, send_by_bytes=1),
        SocketSender(AF_INET, ('localhost', port_number), dgram=1, terminate_seq='\n'),
        SocketSender(AF_INET, ('localhost', port_number), dgram=1, terminate_seq='\0'),
        SocketSender(AF_INET, ('localhost', port_number), dgram=1, terminate_seq='\0\n'),
        SocketSender(AF_INET, ('localhost', port_number), dgram=1, terminate_seq=''),
        SocketSender(AF_INET, ('localhost', port_number), dgram=0),
        SocketSender(AF_INET, ('localhost', port_number), dgram=0, send_by_bytes=1),
        SocketSender(AF_INET, ('localhost', ssl_port_number), dgram=0, ssl=1),
        SocketSender(AF_INET, ('localhost', ssl_port_number), dgram=0, send_by_bytes=1, ssl=1),
        SocketSender(AF_INET, ('localhost', port_number_network), dgram=1, terminate_seq='\n'),
        SocketSender(AF_INET, ('localhost', port_number_network), dgram=0),
        FileSender('log-pipe'),
        FileSender('log-pipe', send_by_bytes=1),
        FileSender('log-padded-pipe', padding=2048),
        FileSender('log-padded-pipe', padding=2048),
        FileSender('log-file'),
        FileSender('log-file', send_by_bytes=1),
    )

    senders_new = (
        SocketSender(AF_INET, ('localhost', port_number_syslog), dgram=1, new_protocol=1, terminate_seq=''),
        SocketSender(AF_INET, ('localhost', port_number_syslog), dgram=0, new_protocol=1, terminate_seq=''),
    )

    expected = []
    for s in senders:
        expected.extend(s.sendMessages(message))

    expected_new = []
    for s_n in senders_new:
        expected_new.extend(s_n.sendMessages(message_new))

    return (check_file_expected("test-input1", expected, settle_time=6) and
            check_file_expected("test-input1_new", expected_new, settle_time=6, syslog_prefix=syslog_new_prefix, skip_prefix=len('<7>1 ')))

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
      'final4',
      'final5'
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

