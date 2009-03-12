#!/usr/bin/env python

import os, sys, signal, traceback, time, errno
from socket import *
import struct

padding = 'x' * 250
session_counter = 0
need_to_flush = False

class MessageSender:
    def __init__(self, repeat=100):
        self.repeat = repeat
        
    def sendMessages(self, msg):
        global session_counter
        self.initSender()
        expected = []
        for counter in range(1, self.repeat):
            if msg[0] != '<':
                line = '<7>%s %d/%d %s' % (msg, session_counter, counter, padding)
            else:
                line = '%s %d/%d %s' % (msg, session_counter, counter, padding)
            self.sendMessage(line)
        if msg[0] == '<':
            msg = msg[msg.index('>')+1:]
        expected.append((msg, session_counter, self.repeat))
        session_counter = session_counter + 1
        # avoid message reordering
        time.sleep(1)
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
        global need_to_flush

        need_to_flush = True
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

def flush_files():
    global syslogng_pid, need_to_flush

    if syslogng_pid == 0 or not need_to_flush:
        return True

    # sendMessages waits between etaps, so we assume that syslog-ng has
    # already received/processed everything we've sent to it. Go ahead send
    # a HUP signal.
    try:
        os.kill(syslogng_pid, signal.SIGHUP)
    except OSError:
        print "Error sending HUP signal to syslog-ng"
        raise
    # allow syslog-ng to perform config reload
    time.sleep(3)
    need_to_flush = False
    

def readpidfile(pidfile):
    f = open(pidfile, 'r')
    pid = f.read()
    f.close()
    return int(pid.strip())

def check_expected(fname, messages):
    flush_files()
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


drvtest_conf = """@version: 3.0

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

def test_input_drivers():
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

    return check_expected("test-input1.log", expected);


def test_indep():
    message = '2004-09-07T10:43:21+01:00 bzorp prog: indep pipes';
    
    s = SocketSender(AF_UNIX, 'log-stream', dgram=0, repeat=10)
    expected = s.sendMessages(message)
    return check_expected("test-indep1.log", expected) and check_expected("test-indep2.log", expected)

def test_final():
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
    
    for ndx in range(0, len(messages)):
        if not check_expected('test-final%d.log' % (ndx + 1,), expected[ndx]):
            return False
    return True

def test_fallback():
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
    
    for ndx in range(0, len(messages)):
        if not check_expected('test-fb%d.log' % (ndx + 1,), expected[ndx]):
            return False
    return True

def test_catchall():

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
        
    return check_expected("test-catchall.log", expected);

filter_conf = """@version: 3.0

options { ts_format(iso); chain_hostnames(no); keep_hostname(yes); };

source s_int { internal(); };
source s_unix { unix-stream("log-stream"); };

filter f_facility { message("facility"); };
filter f_facility1 { facility(syslog); };
filter f_facility2 { facility(kern); };
filter f_facility3 { facility(mail); };
filter f_facility4 { facility(daemon,auth,lpr); };

destination d_facility1 { file("test-facility1.log"); };
destination d_facility2 { file("test-facility2.log"); };
destination d_facility3 { file("test-facility3.log"); };
destination d_facility4 { file("test-facility4.log"); };

log { source(s_unix); 
    filter(f_facility);
    log { filter(f_facility1); destination(d_facility1); };
    log { filter(f_facility2); destination(d_facility2); };
    log { filter(f_facility3); destination(d_facility3); };
    log { filter(f_facility4); destination(d_facility4); };
};

filter f_level { message("level"); };
filter f_level1 { level(debug); };
filter f_level2 { level(info); };
filter f_level3 { level(notice); };
filter f_level4 { level(warning..crit); };

destination d_level1 { file("test-level1.log"); };
destination d_level2 { file("test-level2.log"); };
destination d_level3 { file("test-level3.log"); };
destination d_level4 { file("test-level4.log"); };

log { source(s_unix); 
    filter(f_level);
    log { filter(f_level1); destination(d_level1); };
    log { filter(f_level2); destination(d_level2); };
    log { filter(f_level3); destination(d_level3); };
    log { filter(f_level4); destination(d_level4); };
};

"""

def test_facility_single():
    messages = (
      '<41>2004-09-07T10:43:21+01:00 bzorp prog: facility1',
      '<1>2004-09-07T10:43:21+01:00 bzorp prog: facility2',
      '<17>2004-09-07T10:43:21+01:00 bzorp prog: facility3',
    )
    expected = [None,] * len(messages)
    
    s = SocketSender(AF_UNIX, 'log-stream', dgram=0, repeat=10)
    for ndx in range(0, len(messages)):
        if not expected[ndx]:
            expected[ndx] = []
        expected[ndx].extend(s.sendMessages(messages[ndx]))
    
    for ndx in range(0, len(messages)):
        if not check_expected('test-facility%d.log' % (ndx + 1,), expected[ndx]):
            return False
    return True

def test_facility_multi():
    messages = (
      '<25>2004-09-07T10:43:21+01:00 bzorp prog: facility4',
      '<33>2004-09-07T10:43:21+01:00 bzorp prog: facility4',
      '<49>2004-09-07T10:43:21+01:00 bzorp prog: facility4',
    )
    expected = []
    
    s = SocketSender(AF_UNIX, 'log-stream', dgram=0, repeat=10)
    for ndx in range(0, len(messages)):
        expected.extend(s.sendMessages(messages[ndx]))
    
    if not check_expected('test-facility4.log', expected):
        return False
    return True


def test_level_single():
    messages = (
      '<7>2004-09-07T10:43:21+01:00 bzorp prog: level1',
      '<6>2004-09-07T10:43:21+01:00 bzorp prog: level2',
      '<5>2004-09-07T10:43:21+01:00 bzorp prog: level3',
    )
    expected = [None,] * len(messages)
    
    s = SocketSender(AF_UNIX, 'log-stream', dgram=0, repeat=10)
    for ndx in range(0, len(messages)):
        if not expected[ndx]:
            expected[ndx] = []
        expected[ndx].extend(s.sendMessages(messages[ndx]))
    
    for ndx in range(0, len(messages)):
        if not check_expected('test-level%d.log' % (ndx + 1,), expected[ndx]):
            return False
    return True

def test_level_multi():
    messages = (
      '<4>2004-09-07T10:43:21+01:00 bzorp prog: level4',
      '<3>2004-09-07T10:43:21+01:00 bzorp prog: level4',
      '<2>2004-09-07T10:43:21+01:00 bzorp prog: level4',
    )
    expected = []
    
    s = SocketSender(AF_UNIX, 'log-stream', dgram=0, repeat=10)
    for ndx in range(0, len(messages)):
        expected.extend(s.sendMessages(messages[ndx]))
    
    if not check_expected('test-level4.log', expected):
        return False
    return True


tests = (
  (drvtest_conf, (test_catchall, test_input_drivers, test_fallback, test_final, test_indep)),
  (filter_conf, (test_facility_single, test_facility_multi, test_level_single, test_level_multi)),
)

create_pipes()

syslogng_pid = 0

try:
    for (conf, test_cases) in tests:
        for test in test_cases:
            if not start_syslogng(conf):
                sys.exit(1)
            
            if not test():
                print("FAIL: test %s failed" % test.__name__)
                sys.exit(1)
            if not stop_syslogng():
                sys.exit(1)
finally:
    stop_syslogng()

