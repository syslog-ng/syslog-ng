#!/usr/bin/python

import os, sys, signal, traceback, time
from socket import *

class MessageSender:
    def __init__(self, repeat=1000):
        self.repeat = repeat
        
    def sendMessages(self, messages):
        self.initSender()
        for msg in messages:
            for counter in range(1, self.repeat):
                self.sendMessage('%s %d %s' % (msg, counter, 'x' * 1500))


class SocketSender(MessageSender):
    def __init__(self, family, sock_name, dgram=0, send_by_bytes=0, terminate_seq='\n', repeat=1000):
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
    
    def sendMessage(self, msg):
        line = '%s%s' % (msg, self.terminate_seq)
        if self.send_by_bytes:
            for c in line:
                self.sock.send(c)
        else:
            self.sock.send(line)
            

def readpidfile(pidfile):
    f = open(pidfile, 'r')
    pid = f.read()
    f.close()
    return int(pid.strip())

messages = (
    '<7>2004-09-07T10:43:21.156+01:00 bzorp prog: msg',
)

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
)

conf = """
source s_int { internal(); };
source s_unix { unix-stream("log-stream"); unix-dgram("log-dgram");  };
source s_inet { tcp(port(2000)); udp(port(2000)); };

destination dst { file("test-output.log"); };

log { source(s_int); source(s_unix); source(s_inet); destination(dst); };
"""

f = open('test.conf', 'w')
f.write(conf)
f.close()

try:
  os.unlink('test-output.log')
except OSError:
  pass
rc = os.system('../../src/syslog-ng -f test.conf -p syslog-ng.pid')
if rc != 0:
    print "Error in syslog-ng startup"
    success = 0
else:
    success = 1

if success:
    try:
        pid = readpidfile('syslog-ng.pid')

        for s in senders:
            s.sendMessages(messages)
        time.sleep(1)
    finally:
        try:
            os.kill(pid, signal.SIGTERM)
        except OSError:
            print "Syslog-ng exited before SIGTERM..."
            success = 0

sys.exit(not success)

