#!/usr/bin/env python

import os, sys, signal, traceback, time, errno
from socket import *
import struct, stat, re

logstore_store_supported = False
wildcard_file_source_supported = False


padding = 'x' * 250
session_counter = 0
need_to_flush = False

syslog_prefix = "2004-09-07T10:43:21+01:00 bzorp prog:"

def print_user(msg):
    print '    ', time.strftime('%Y-%M-%dT%H:%m:%S'), msg

def print_start(testcase):
    print("\n\n##############################################")
    print("### Starting testcase: %s" % test.__name__)
    print("##############################################")
    print_user("Testcase start")

def print_end(testcase, result):
    print_user("Testcase end")
    print("##############################################")
    if result:
        print("### PASS: %s" % test.__name__)
    else:
        print("### FAIL: %s" % test.__name__)
    print("##############################################\n\n")


class MessageSender(object):
    def __init__(self, repeat=100):
        self.repeat = repeat
        
    def sendMessages(self, msg, pri=7):
        global session_counter
        global need_to_flush
        global syslog_prefix

        need_to_flush = True

        print_user("generating %d messages using transport %s" % (self.repeat, str(self)))

        self.initSender()
        expected = []

        for counter in range(1, self.repeat):
            line = '<%d>%s %s %d/%d %s %s' % (pri, syslog_prefix, msg, session_counter, counter, str(self), padding)
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
                        print_user('got ENOBUFS, sleeping...')
                        time.sleep(0.5)
                        repeat = True
                    else:
                        print_user("hmm... got an error to the 'send' call, maybe syslog-ng is not accepting messages?")
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
                        print_user('got ENOBUFS, sleeping...')
                        time.sleep(0.5)
                        repeat = True
                    else:
                        print_user("hmm... got an error to the 'send' call, maybe syslog-ng is not accepting messages?")
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


class FileSender(MessageSender):
    def __init__(self, file_name, padding=0, send_by_bytes=0, terminate_seq='\n', repeat=100):
        MessageSender.__init__(self, repeat)
        self.file_name = file_name
        self.padding = padding
        self.send_by_bytes = send_by_bytes
        self.terminate_seq = terminate_seq
        self.fd = None
        
        try:
            if stat.S_ISFIFO(os.stat(file_name).st_mode):
                self.is_pipe = True
            else:
                self.is_pipe = False
        except OSError:
            self.is_pipe = False

    def __del__(self):
        if self.fd:
            self.fd.flush()
            self.fd.close()
        
    def initSender(self):
        if self.is_pipe:
            self.fd = open(self.file_name, "w")
        else:
            self.fd = open(self.file_name, "a")

    def sendMessages(self, msg):
        res = super(FileSender, self).sendMessages(msg)

        return res
    
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
        if self.is_pipe:
            if self.padding:
                return 'pipe(%s[%d])' % (self.file_name, self.padding)
            else:
                return 'pipe(%s)' % (self.file_name,)
        else:
            return 'file(%s)' % (self.file_name,)


def init_env():
    for pipe in ('log-pipe', 'log-padded-pipe'):
        try:
            os.unlink(pipe)
        except OSError:
            pass
        os.mkfifo(pipe)
    try:
        os.mkdir("wildcard")
    except OSError, e:
        if e.errno != errno.EEXIST:
            raise

def is_premium():
    version = os.popen('../../src/syslog-ng -V', 'r').read()
    if version.find('premium-edition') != -1:
        return True
    return False

def start_syslogng(conf, keep_persist=False, verbose=False):
    global syslogng_pid
    
    os.system('rm -f test-*.log test-*.lgs wildcard/* log-file')
    if not keep_persist:
        os.system('rm -f syslog-ng.persist')

    if not logstore_store_supported:
        conf = re.sub('logstore\(.*\);', '', conf)

    f = open('test.conf', 'w')
    f.write(conf)
    f.close()

    if verbose:
        verbose_opt = '-edv'
    else:
        verbose_opt = '-e'
    
    syslogng_pid = os.fork()
    if syslogng_pid == 0:
        rc = os.execl('../../src/syslog-ng', '../../src/syslog-ng', '-f', 'test.conf', '--fd-limit', '1024', '-F', verbose_opt, '-p', 'syslog-ng.pid', '-R', 'syslog-ng.persist', '--no-caps', '--enable-core')
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
    print_user("syslog-ng exited with a non-zero value")
    return False

def flush_files(settle_time=1):
    global syslogng_pid, need_to_flush

    if syslogng_pid == 0 or not need_to_flush:
        return True

    print_user("waiting for syslog-ng to settle down before SIGHUP (%d secs)" % settle_time)
    # allow syslog-ng to settle
    time.sleep(settle_time)

    # sendMessages waits between etaps, so we assume that syslog-ng has
    # already received/processed everything we've sent to it. Go ahead send
    # a HUP signal.
    try:
        print_user("Sending syslog-ng the HUP signal (pid: %d)" % syslogng_pid)
        os.kill(syslogng_pid, signal.SIGHUP)
    except OSError:
        print_user("Error sending HUP signal to syslog-ng")
        raise
    # allow syslog-ng to perform config reload & file flush
    print_user("waiting for syslog-ng to process SIGHUP (%d secs)" % 1)
    time.sleep(1)
    need_to_flush = False
    

def readpidfile(pidfile):
    f = open(pidfile, 'r')
    pid = f.read()
    f.close()
    return int(pid.strip())

def check_expected(fname, messages, settle_time=1):

    flush_files(settle_time)
    print_user("Checking contents of output file: %s" % fname)
    if logstore_store_supported:
        iter = (False, True)
    else:
        iter = (False,)
    for logstore in iter:
            try:
                if logstore:
                        f = os.popen("../../src/logcat %s.lgs" % fname, "r")
                else:
                        f = open(fname + ".log", "r")
            except (IOError, OSError), e:
                print_user("Error opening file: %s, %s" % (fname, str(e)))
                return False

            lineno = 1
            # we handle message reordering by going through the complete
            # file and looking for the message
            # end the collected ranges must span the number of messages that
            # we expect
            matches = {}

            read_line = f.readline().strip()
            while read_line:
                if read_line[:len(syslog_prefix)] != syslog_prefix:
                    print_user("message does not start with syslog_prefix, file=%s:%d, line=%s" % (fname, lineno, read_line))
                    return False
                msg = read_line[len(syslog_prefix):]
                m = re.match("^ (\S+) (\d+)/(\d+)", msg)

                if not m:
                    print_user("message payload unexpected format, file=%s:%d, line=%s" % (fname, lineno, read_line))
                    return False
                msg = m.group(1)
                session = int(m.group(2))
                id = int(m.group(3))
                if not matches.has_key((msg, session)):
                    if id != 1:
                        print_user("the id of the first message in a session is not 1, session=%d, id=%d, file=%s:%d, line=%s"  % (session, id, fname, lineno, read_line))
                        return False
                else:
                    if matches[(msg, session)] != id - 1:
                        print_user("message reordering/drop detected in the same session, session=%d, id=%d, expected_id=%d, file=%s:%d, line=%s" % (session, id, matches[(msg, session)]+1, fname, lineno, read_line))
                        return False
                matches[(msg, session)] = id
                read_line = f.readline().strip()
                lineno = lineno + 1

            for (msg, session, count) in messages:

                if not matches.has_key((msg, session)):
                    print_user("output log files lack this kind of message: %s session: %d, count: %d" % (msg, session, count))
                    return False
                if matches[(msg, session)] != count - 1:
                    print_user("not enough messages found, message: %s, session: %d, last_id: %d, count: %d" % (msg, session,  matches[(msg, session)], count))
                    return False
    return True


drvtest_conf = """@version: 3.0

options { ts_format(iso); chain_hostnames(no); keep_hostname(yes); };

source s_int { internal(); };
source s_unix { unix-stream("log-stream"); unix-dgram("log-dgram");  };
source s_inet { tcp(port(2000)); udp(port(2000) so_rcvbuf(131072)); };
source s_pipe { pipe("log-pipe"); pipe("log-padded-pipe" pad_size(2048)); };
source s_file { file("log-file"); };
source s_catchall { unix-stream("log-stream-catchall"); };

filter f_input1 { message("input_drivers"); };
destination d_input1 { file("test-input1.log"); logstore("test-input1.lgs"); };

log { source(s_int); source(s_unix); source(s_inet); source(s_pipe); source(s_file);
        log { filter(f_input1); destination(d_input1); };
};


destination d_indep1 { file("test-indep1.log"); logstore("test-indep1.lgs"); };
destination d_indep2 { file("test-indep2.log"); logstore("test-indep2.lgs"); };

filter f_indep { message("indep_pipes"); };

# test independent log pipes
log { source(s_int); source(s_unix); source(s_inet); filter(f_indep);
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

destination d_fb1 { file("test-fb1.log"); logstore("test-fb1.lgs"); };
destination d_fb2 { file("test-fb2.log"); logstore("test-fb2.lgs"); };
destination d_fb3 { file("test-fb3.log"); logstore("test-fb3.lgs"); };
destination d_fb4 { file("test-fb4.log"); logstore("test-fb4.lgs"); };

# test fallback flag
log { source(s_int); source(s_unix); source(s_inet); filter(f_fb);
        log { destination(d_fb4); flags(fallback); };
        log { filter(f_fb1); destination(d_fb1); };
        log { filter(f_fb2); destination(d_fb2); };
        log { filter(f_fb3); destination(d_fb3); };
};
        
# test catch-all flag
filter f_catchall { message("catchall"); };

destination d_catchall { file("test-catchall.log"); logstore("test-catchall.lgs"); };

log { filter(f_catchall); destination(d_catchall); flags(catch-all); };
"""

def test_input_drivers():
    message = 'input_drivers';

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

    return check_expected("test-input1", expected, settle_time=6);


def test_indep():
    message = 'indep_pipes';
    
    s = SocketSender(AF_UNIX, 'log-stream', dgram=0, repeat=10)
    expected = s.sendMessages(message)
    return check_expected("test-indep1", expected) and check_expected("test-indep2", expected)

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
        if not check_expected('test-final%d' % (ndx + 1,), expected[ndx]):
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
        if not check_expected('test-fb%d' % (ndx + 1,), expected[ndx]):
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
        SocketSender(AF_INET, ('localhost', 2000), dgram=1),
        SocketSender(AF_INET, ('localhost', 2000), dgram=1, terminate_seq='\0'),
        SocketSender(AF_INET, ('localhost', 2000), dgram=1, terminate_seq='\0\n'),
        SocketSender(AF_INET, ('localhost', 2000), dgram=0),
        SocketSender(AF_INET, ('localhost', 2000), dgram=0, send_by_bytes=1),
        FileSender('log-pipe'),
        FileSender('log-pipe', send_by_bytes=1),
        FileSender('log-padded-pipe', padding=2048),
        FileSender('log-padded-pipe', padding=2048),
    )
        
    expected = []
    for s in senders:
        expected.extend(s.sendMessages(message))
        
    return check_expected("test-catchall", expected);

filter_conf = """@version: 3.0

options { ts_format(iso); chain_hostnames(no); keep_hostname(yes); };

source s_int { internal(); };
source s_unix { unix-stream("log-stream"); };

filter f_facility { message("facility"); };
filter f_facility1 { facility(syslog); };
filter f_facility2 { facility(kern); };
filter f_facility3 { facility(mail); };
filter f_facility4 { facility(daemon,auth,lpr); };

destination d_facility1 { file("test-facility1.log"); logstore("test-facility1.lgs"); };
destination d_facility2 { file("test-facility2.log"); logstore("test-facility2.lgs"); };
destination d_facility3 { file("test-facility3.log"); logstore("test-facility3.lgs"); };
destination d_facility4 { file("test-facility4.log"); logstore("test-facility4.lgs"); };

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

destination d_level1 { file("test-level1.log"); logstore("test-level1.lgs"); };
destination d_level2 { file("test-level2.log"); logstore("test-level2.lgs"); };
destination d_level3 { file("test-level3.log"); logstore("test-level3.lgs"); };
destination d_level4 { file("test-level4.log"); logstore("test-level4.lgs"); };

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
      (41, 'facility1'),
      (1, 'facility2'),
      (17, 'facility3'),
    )
    expected = [None,] * len(messages)
    
    s = SocketSender(AF_UNIX, 'log-stream', dgram=0, repeat=10)
    for ndx in range(0, len(messages)):
        if not expected[ndx]:
            expected[ndx] = []
        expected[ndx].extend(s.sendMessages(messages[ndx][1], pri=messages[ndx][0]))
    
    for ndx in range(0, len(messages)):
        if not check_expected('test-facility%d' % (ndx + 1,), expected[ndx]):
            return False
    return True

def test_facility_multi():
    messages = (
      (25, 'facility4'),
      (33, 'facility4'),
      (49, 'facility4'),
    )
    expected = []
    
    s = SocketSender(AF_UNIX, 'log-stream', dgram=0, repeat=10)
    for ndx in range(0, len(messages)):
        expected.extend(s.sendMessages(messages[ndx][1], pri=messages[ndx][0]))
    
    if not check_expected('test-facility4', expected):
        return False
    return True


def test_level_single():
    messages = (
      (7, 'level1'),
      (6, 'level2'),
      (5, 'level3'),
    )
    expected = [None,] * len(messages)
    
    s = SocketSender(AF_UNIX, 'log-stream', dgram=0, repeat=10)
    for ndx in range(0, len(messages)):
        if not expected[ndx]:
            expected[ndx] = []
        expected[ndx].extend(s.sendMessages(messages[ndx][1], pri=messages[ndx][0]))
    
    for ndx in range(0, len(messages)):
        if not check_expected('test-level%d' % (ndx + 1,), expected[ndx]):
            return False
    return True

def test_level_multi():
    messages = (
      (4, 'level4'),
      (3, 'level4'),
      (2, 'level4'),
    )
    expected = []
    
    s = SocketSender(AF_UNIX, 'log-stream', dgram=0, repeat=10)
    for ndx in range(0, len(messages)):
        expected.extend(s.sendMessages(messages[ndx][1], pri=messages[ndx][0]))

    if not check_expected('test-level4', expected):
        return False
    return True

wildcard_conf = """@version: 3.0

options { ts_format(iso); chain_hostnames(no); keep_hostname(yes); };

source s_int { internal(); };
source s_wildcard { file("wildcard/*.log"); };

destination d_wildcard { file("test-wildcard.log"); logstore("test-wildcard.lgs"); };

log { source(s_wildcard); destination(d_wildcard); };

"""

def test_wildcard_files():
    messages = (
      'wildcard0',
      'wildcard1',
      'wildcard2',
      'wildcard3',
      'wildcard4',
      'wildcard5',
      'wildcard6',
      'wildcard7',
    )
    
    if not wildcard_file_source_supported:
        print_user("Not testing a Premium version, skipping wild card source tests")
        return True
    expected = []

    for ndx in range(0, len(messages)):
        s = FileSender('wildcard/%d.log' % (ndx % 4), repeat=100)
        expected.extend(s.sendMessages(messages[ndx]))

    # we need more time to settle, as poll based polling might need 4*3 sec
    # to read the contents for all 4 files (1 sec to discover there are
    # messages)

    if not check_expected('test-wildcard', expected, settle_time=12):
        return False
    return True


tests = (
  (drvtest_conf, (
     test_input_drivers,
     test_catchall,
     test_fallback,
     test_final,
     test_indep
  )),
  (filter_conf, (
    test_facility_single,
    test_facility_multi,
    test_level_single,
    test_level_multi)),
  (wildcard_conf, (
    test_wildcard_files,)),
)

init_env()

if is_premium():
    logstore_store_supported = True
    wildcard_file_source_supported = True

syslogng_pid = 0

verbose = False
success = True
if len(sys.argv) > 1:
    verbose = True
try:
    for (conf, test_cases) in tests:
        for test in test_cases:
            print_start(test.__name__)

            if not start_syslogng(conf, verbose):
                sys.exit(1)
            print_user("Syslog-ng started")

            print_user("Starting test...")
            if not test():
                print_end(test.__name__, False)
                success = False

            if not stop_syslogng():
                sys.exit(1)
            print_user("syslog-ng stopped")
            print_end(test.__name__, True)
finally:
    stop_syslogng()

if not success:
    sys.exit(1)
