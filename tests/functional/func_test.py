#!/usr/bin/env python

import os, sys, signal, traceback, time, errno
from socket import *
import struct

padding = 'x' * 250
session_counter = 0

class MessageSender:
    def __init__(self, repeat=100):
        self.repeat = repeat
        
    def sendMessages(self, msg):
        global session_counter
        self.initSender()
        expected = []
        for counter in range(1, self.repeat):
            line = '<7>%s %d/%d %s' % (msg, session_counter, counter, padding)
            self.sendMessage(line)
        expected.append((msg, session_counter, self.repeat))
        session_counter = session_counter + 1
        return expected


class SocketSender(MessageSender):
    def __init__(self, family, sock_name, dgram=0, send_by_bytes=0, terminate_seq='\n', repeat=100):
        MessageSender.__init__(self, repeat)
        self.family = family
        self.sock_name = sock_name
        self.sock = None
        self.dgram = dgram
        self.send_by_bytes = send_by_bytes
        self.terminate_seq = terminate_seq
        
    def initSender(self):
        if self.dgram:
            self.sock = socket(self.family, SOCK_DGRAM)
        else:
            self.sock = socket(self.family, SOCK_STREAM)
        
        self.sock.connect(self.sock_name)
        if self.dgram:
                self.sock.send('')
        if sys.platform == 'linux2':
                self.sock.setsockopt(SOL_SOCKET, SO_SNDTIMEO, struct.pack('ll', 3, 0))


    def sendMessage(self, msg):
        line = '%s%s' % (msg, self.terminate_seq)
        if self.send_by_bytes:
            for c in line:
                try:
                    self.sock.send(c)
                except error, e:
                    if e[0] == errno.ENOBUFS:
                        print 'got ENOBUFS, sleeping...'
                        time.sleep(0.5)
                        repeat = True
                    else:
                        print "hmm... got an error to the 'send' call, maybe syslog-ng is not accepting messages?"
                        raise
        else:
            repeat = True
            while repeat:
                try:
                    repeat = False
                    self.sock.send(line)
                    
                    if self.dgram:
                        time.sleep(0.01)
                except error, e:
                    if e[0] == errno.ENOBUFS:
                        print 'got ENOBUFS, sleeping...'
                        time.sleep(0.5)
                        repeat = True
                    else:
                        print "hmm... got an error to the 'send' call, maybe syslog-ng is not accepting messages?"
                        raise

    def __str__(self):
        if self.family == AF_UNIX:
            if self.dgram:
                return 'unix-dgram(%s)' % (self.sock_name)
            else:
                return 'unix-stream(%s)' % (self.sock_name)
        else:
            if self.dgram:
                return 'udp(%s)' % (self.sock_name,)
            else:
                return 'tcp(%s)' % (self.sock_name,)


class PipeSender(MessageSender):
    def __init__(self, pipe_name, padding=0, send_by_bytes=0, terminate_seq='\n', repeat=100):
        MessageSender.__init__(self, repeat)
        self.pipe_name = pipe_name
        self.padding = padding
        self.send_by_bytes = send_by_bytes
        self.terminate_seq = terminate_seq
        self.fd = None
        
    def __del__(self):
        if self.fd:
            self.fd.flush()
            self.fd.close()
        
    def initSender(self):
        self.fd = open(self.pipe_name, "w")
    
    def sendMessage(self, msg):
        line = '%s%s' % (msg, self.terminate_seq)
        if self.padding:
            line += '\0' * (self.padding - len(line))
        if self.send_by_bytes:
            for c in line:
                self.fd.write(c)
                self.fd.flush()
        else:
            self.fd.write(line)
            self.fd.flush()

    def __str__(self):
        if self.padding:
            return 'pipe(%s[%d])' % (self.pipe_name, self.padding)
        else:
            return 'pipe(%s)' % (self.pipe_name,)

def create_pipes():
    for pipe in ('log-pipe', 'log-padded-pipe'):
        try:
            os.unlink(pipe)
        except OSError:
            pass
        os.mkfifo(pipe)

def start_syslogng(conf):
    global syslogng_pid
    
    os.system('rm test-*.log')
    f = open('test.conf', 'w')
    f.write(conf)
    f.close()
    
    syslogng_pid = os.fork()
    if syslogng_pid == 0:
        rc = os.execl('../../src/syslog-ng', '../../src/syslog-ng', '-f', 'test.conf', '-Fev', '-p', 'syslog-ng.pid', '--no-caps', '--enable-core')
        sys.exit(rc)
    time.sleep(3)
    return True

def stop_syslogng():
    global syslogng_pid
    
    if syslogng_pid == 0:
        return True

    try:
        os.kill(syslogng_pid, signal.SIGTERM)
    except OSError:
        pass
    try:
        try:
            (pid, rc) = os.waitpid(syslogng_pid, 0)
        except OSError:
            raise
    finally:
        syslogng_pid = 0
    if rc == 0:
        return True
    print "syslog-ng exited with a non-zero value"
    return False

    

def readpidfile(pidfile):
    f = open(pidfile, 'r')
    pid = f.read()
    f.close()
    return int(pid.strip())

def check_expected(fname, messages):
    try:
        f = open(fname, "r")
    except IOError:
        print "Error opening file: %s" % (fname,)
        return False
        
    lineno = 0
    for (msg, session, count) in messages:
        for i in range(1, count):
            expected_line = '%s %d/%d %s' % (msg, session, i, padding)
            read_line = f.readline().strip()
            lineno += 1
            if expected_line != read_line:
                print "Unexpected line in log file, file=%s:%d, expected=%s, actual=%s" % (fname, lineno, expected_line, read_line)
                return False
    return True


conf = """@version: 3.0

options { ts_format(iso); chain_hostnames(no); keep_hostname(yes); };

source s_int { internal(); };
source s_unix { unix-stream("log-stream"); unix-dgram("log-dgram");  };
source s_inet { tcp(port(2000)); udp(port(2000) so_rcvbuf(131072)); };
source s_pipe { pipe("log-pipe"); pipe("log-padded-pipe" pad_size(2048)); };
source s_catchall { unix-stream("log-stream-catchall"); };

filter f_input1 { message("input drivers"); };
destination d_input1 { file("test-input1.log"); };

log { source(s_int); source(s_unix); source(s_inet); source(s_pipe);
        log { filter(f_input1); destination(d_input1); };
};


destination d_indep1 { file("test-indep1.log"); };
destination d_indep2 { file("test-indep2.log"); };

filter f_indep { message("indep pipes"); };

# test independent log pipes
log { source(s_int); source(s_unix); source(s_inet); filter(f_indep);
        log { destination(d_indep1); };
        log { destination(d_indep2); };
};

filter f_final { message("final"); };

filter f_final1 { message("final1"); };
filter f_final2 { message("final2"); };
filter f_final3 { message("final3"); };

destination d_final1 { file("test-final1.log"); };
destination d_final2 { file("test-final2.log"); };
destination d_final3 { file("test-final3.log"); };
destination d_final4 { file("test-final4.log"); };


# test final flag + rest
log { source(s_int); source(s_unix); source(s_inet);  filter(f_final);
        log { filter(f_final1); destination(d_final1); flags(final); };
        log { filter(f_final2); destination(d_final2); flags(final); };
        log { filter(f_final3); destination(d_final3); flags(final); };
        log { destination(d_final4); };
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
log { source(s_int); source(s_unix); source(s_inet); filter(f_fb);
        log { destination(d_fb4); flags(fallback); };
        log { filter(f_fb1); destination(d_fb1); };
        log { filter(f_fb2); destination(d_fb2); };
        log { filter(f_fb3); destination(d_fb3); };
};
        
# test catch-all flag
filter f_catchall { message("catchall"); };

destination d_catchall { file("test-catchall.log"); };

log { filter(f_catchall); destination(d_catchall); flags(catch-all); };
"""

def test_input_drivers(conf):
    message = '2004-09-07T10:43:21+01:00 bzorp prog: input drivers';

    senders = (
        SocketSender(AF_UNIX, 'log-dgram', dgram=1),
        SocketSender(AF_UNIX, 'log-dgram', dgram=1, terminate_seq='\0'),
        SocketSender(AF_UNIX, 'log-dgram', dgram=1, terminate_seq='\0\n'),
        SocketSender(AF_UNIX, 'log-stream', dgram=0),
        SocketSender(AF_UNIX, 'log-stream', dgram=0, send_by_bytes=1),
        SocketSender(AF_INET, ('localhost', 2000), dgram=1),
        SocketSender(AF_INET, ('localhost', 2000), dgram=1, terminate_seq='\0'),
        SocketSender(AF_INET, ('localhost', 2000), dgram=1, terminate_seq='\0\n'),
        SocketSender(AF_INET, ('localhost', 2000), dgram=0),
        SocketSender(AF_INET, ('localhost', 2000), dgram=0, send_by_bytes=1),
        PipeSender('log-pipe'),
        PipeSender('log-pipe', send_by_bytes=1),
        PipeSender('log-padded-pipe', padding=2048),
        PipeSender('log-padded-pipe', padding=2048),
    )
        
    expected = []
    for s in senders:
        print("INFO: Generating messages using: %s" % str(s))
        expected.extend(s.sendMessages('%s %s' % (message, s)))
        # allow buffers to be emptied
        time.sleep(1)
        
    time.sleep(1)

    return check_expected("test-input1.log", expected);


def test_indep(conf):
    message = '2004-09-07T10:43:21+01:00 bzorp prog: indep pipes';
    
    s = SocketSender(AF_UNIX, 'log-stream', dgram=0, repeat=10)
    expected = s.sendMessages(message)
    time.sleep(1)
    return check_expected("test-indep1.log", expected) and check_expected("test-indep2.log", expected)

def test_final(conf):
    messages = (
      '2004-09-07T10:43:21+01:00 bzorp prog: final1',
      '2004-09-07T10:43:21+01:00 bzorp prog: final2',
      '2004-09-07T10:43:21+01:00 bzorp prog: final3',
      '2004-09-07T10:43:21+01:00 bzorp prog: final4'
    )
    expected = [None,] * len(messages)
    
    s = SocketSender(AF_UNIX, 'log-stream', dgram=0, repeat=10)
    for ndx in range(0, len(messages)):
        if not expected[ndx]:
            expected[ndx] = []
        expected[ndx].extend(s.sendMessages(messages[ndx]))
    time.sleep(1)
    
    for ndx in range(0, len(messages)):
        if not check_expected('test-final%d.log' % (ndx + 1,), expected[ndx]):
            return False
    return True

def test_fallback(conf):
    messages = (
      '2004-09-07T10:43:21+01:00 bzorp prog: fallback1',
      '2004-09-07T10:43:21+01:00 bzorp prog: fallback2',
      '2004-09-07T10:43:21+01:00 bzorp prog: fallback3',
      '2004-09-07T10:43:21+01:00 bzorp prog: fallback4',
    )
    expected = [None,] * len(messages)
    
    s = SocketSender(AF_UNIX, 'log-stream', dgram=0, repeat=10)
    for ndx in range(0, len(messages)):
        if not expected[ndx]:
            expected[ndx] = []
        expected[ndx].extend(s.sendMessages(messages[ndx]))
    time.sleep(1)
    
    print expected[0]
    
    for ndx in range(0, len(messages)):
        if not check_expected('test-fb%d.log' % (ndx + 1,), expected[ndx]):
            return False
    return True

def test_catchall(conf):

    message = '2004-09-07T10:43:21+01:00 bzorp prog: catchall';

    senders = (
        SocketSender(AF_UNIX, 'log-stream-catchall', dgram=0, send_by_bytes=1),
        SocketSender(AF_UNIX, 'log-dgram', dgram=1),
        SocketSender(AF_UNIX, 'log-dgram', dgram=1, terminate_seq='\0'),
        SocketSender(AF_UNIX, 'log-dgram', dgram=1, terminate_seq='\0\n'),
        SocketSender(AF_UNIX, 'log-stream', dgram=0),
        SocketSender(AF_UNIX, 'log-stream', dgram=0, send_by_bytes=1),
        SocketSender(AF_INET, ('localhost', 2000), dgram=1),
        SocketSender(AF_INET, ('localhost', 2000), dgram=1, terminate_seq='\0'),
        SocketSender(AF_INET, ('localhost', 2000), dgram=1, terminate_seq='\0\n'),
        SocketSender(AF_INET, ('localhost', 2000), dgram=0),
        SocketSender(AF_INET, ('localhost', 2000), dgram=0, send_by_bytes=1),
        PipeSender('log-pipe'),
        PipeSender('log-pipe', send_by_bytes=1),
        PipeSender('log-padded-pipe', padding=2048),
        PipeSender('log-padded-pipe', padding=2048),
    )
        
    expected = []
    for s in senders:
        print("INFO: Generating messages using: %s" % str(s))
        expected.extend(s.sendMessages('%s %s' % (message, s)))
        # allow buffers to be emptied
        time.sleep(1)
        
    time.sleep(1)

    return check_expected("test-catchall.log", expected);


tests = (test_catchall, test_input_drivers, test_fallback, test_final, test_indep)

create_pipes()

try:
    for test in tests:
        if not start_syslogng(conf):
            sys.exit(1)
        
        if not test(conf):
            print("FAIL: test %s failed" % test.__name__)
            sys.exit(1)
        if not stop_syslogng():
            sys.exit(1)
finally:
    stop_syslogng()

