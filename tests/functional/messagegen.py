import struct, stat, re
from socket import *
import os, sys

from log import *

syslog_prefix = "2004-09-07T10:43:21+01:00 bzorp prog[12345]:"
session_counter = 0
need_to_flush = False
padding = 'x' * 250

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
            line = '<%d>%s %s %03d/%05d %s %s' % (pri, syslog_prefix, msg, session_counter, counter, str(self), padding)
            self.sendMessage(line)
        expected.append((msg, session_counter, self.repeat))
        session_counter = session_counter + 1
        return expected


class SocketSender(MessageSender):
    def __init__(self, family, sock_name, dgram=0, send_by_bytes=0, terminate_seq='\n', repeat=100, ssl=0):
        MessageSender.__init__(self, repeat)
        self.family = family
        self.sock_name = sock_name
        self.sock = None
        self.dgram = dgram
        self.send_by_bytes = send_by_bytes
        self.terminate_seq = terminate_seq
        self.ssl = ssl

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
        if not self.dgram and self.ssl:
                self.sock = ssl(self.sock)


    def sendMessage(self, msg):
        line = '%s%s' % (msg, self.terminate_seq)
        if self.send_by_bytes:
            for c in line:
                try:
                    if self.ssl:
                        self.sock.write(c)
                    else:
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

                    # WTF? SSLObject only has write, whereas sockets only have send methods
                    if self.ssl:
                        self.sock.write(line)
                    else:
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
            elif not self.ssl:
                return 'tcp(%s)' % (self.sock_name,)
            else:
                return 'tls(%s)' % (self.sock_name,)


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

