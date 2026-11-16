#!/usr/bin/env python
#############################################################################
# Copyright (c) 2007-2010 Balabit
# Copyright (c) 2026 OneIdentity
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
import errno
import ssl
import struct
import sys
import time
from socket import AF_UNIX
from socket import error
from socket import SO_SNDTIMEO
from socket import SOCK_DGRAM
from socket import SOCK_STREAM
from socket import socket
from socket import SOL_SOCKET

from .message_sender import MessageSender


class SocketSender(MessageSender):
    """Send messages via Unix domain or Internet sockets.

    Supports:
    - Unix domain sockets (stream and datagram)
    - Internet sockets (TCP and UDP)
    - TLS/SSL connections
    - Byte-by-byte sending with flush control
    - Custom termination sequences
    - Automatic retry on ENOBUFS
    """

    def __init__(
        self,
        family,
        sock_name,
        dgram=0,
        send_by_bytes=0,
        terminate_seq='\n',
        repeat=100,
        ssl_enabled=0,
        new_protocol=0,
    ):
        """Initialize socket sender.

        Args:
            family: Socket family (AF_UNIX or AF_INET)
            sock_name: Socket address (path for Unix, (host, port) tuple for Internet)
            dgram: Use datagram (UDP/UNIX-DGRAM) instead of stream (TCP/UNIX-STREAM)
            send_by_bytes: Write byte-by-byte with flush after each byte
            terminate_seq: Sequence to append after message (default: '\n')
            repeat: Number of messages to send
            ssl_enabled: Use SSL/TLS encryption (TCP only)
            new_protocol: Use RFC5424 format with framing
        """
        MessageSender.__init__(self, repeat, new_protocol, dgram)
        self.family = family
        self.sock_name = sock_name
        self.sock = None
        self.dgram = dgram
        self.send_by_bytes = send_by_bytes
        self.terminate_seq = terminate_seq
        self.ssl_enabled = ssl_enabled
        self.new_protocol = new_protocol

    def initSender(self):
        """Initialize socket connection.

        Creates socket, connects, and optionally wraps in SSL.
        Sets send timeout on Linux systems.
        """
        if self.dgram:
            self.sock = socket(self.family, SOCK_DGRAM)
        else:
            self.sock = socket(self.family, SOCK_STREAM)

        self.sock.connect(self.sock_name)

        # Send empty datagram to establish connection
        if self.dgram:
            self.sock.send(''.encode())

        # Set send timeout on Linux
        if sys.platform == 'linux2' or sys.platform.startswith('linux'):
            self.sock.setsockopt(SOL_SOCKET, SO_SNDTIMEO, struct.pack('ll', 3, 0))

        # Wrap in SSL if requested (stream only)
        if not self.dgram and self.ssl_enabled:
            ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
            ctx.check_hostname = False
            ctx.verify_mode = ssl.CERT_NONE
            self.sock = ctx.wrap_socket(self.sock)

    def sendMessage(self, msg):
        """Send single message via socket.

        Args:
            msg: Formatted message string to send
        """
        line = '%s%s' % (msg, self.terminate_seq)

        if self.send_by_bytes:
            # Send byte-by-byte
            for c in line:
                self._send_with_retry(c.encode())
        else:
            # Send all at once
            self._send_with_retry(line.encode())

            # Add delay for datagrams to avoid overwhelming receiver
            if self.dgram:
                time.sleep(0.01)

    def _send_with_retry(self, data):
        """Send data with automatic retry on ENOBUFS.

        Args:
            data: Bytes to send
        """
        retry = True
        while retry:
            try:
                retry = False

                # SSL sockets use write(), regular sockets use send()
                if self.ssl_enabled:
                    self.sock.write(data)
                else:
                    self.sock.send(data)

            except error as e:
                if e.args[0] == errno.ENOBUFS:
                    # Buffer full, wait and retry
                    time.sleep(0.5)
                    retry = True
                else:
                    raise

    def close(self):
        """Close the socket connection."""
        if self.sock:
            try:
                self.sock.close()
            except Exception:
                pass
            self.sock = None

    def __enter__(self):
        """Context manager entry."""
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit - ensures socket cleanup."""
        self.close()
        return False

    def __str__(self):
        """Return string representation of sender type."""
        if self.family == AF_UNIX:
            if self.dgram:
                return 'unix-dgram(%s)' % (self.sock_name,)
            else:
                return 'unix-stream(%s)' % (self.sock_name,)
        else:
            if self.dgram:
                return 'udp(%s)' % (self.sock_name,)
            elif not self.ssl_enabled:
                return 'tcp(%s)' % (self.sock_name,)
            else:
                return 'tls(%s)' % (self.sock_name,)
