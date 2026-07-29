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
import os
import stat

from .message_sender import MessageSender


class FileSender(MessageSender):
    """Send messages to file or named pipe with fine-grained control.

    Supports:
    - Named pipes (FIFO) with automatic detection
    - Regular files with append mode
    - Byte-by-byte writing with flush control
    - Null padding to fixed message size
    - Custom termination sequences
    """

    def __init__(self, file_name, padding=0, send_by_bytes=0, terminate_seq='\n', repeat=100):
        """Initialize file sender.

        Args:
            file_name: Path to file or named pipe
            padding: Pad message with null bytes to this size (0=no padding)
            send_by_bytes: Write byte-by-byte with flush after each byte
            terminate_seq: Sequence to append after message (default: '\n')
            repeat: Number of messages to send
        """
        MessageSender.__init__(self, repeat)
        self.file_name = file_name
        self.padding = padding
        self.send_by_bytes = send_by_bytes
        self.terminate_seq = terminate_seq
        self.fd = None

        # Detect if target is a named pipe
        try:
            if stat.S_ISFIFO(os.stat(file_name).st_mode):
                self.is_pipe = True
            else:
                self.is_pipe = False
        except OSError:
            self.is_pipe = False

    def close(self):
        """Flush and close the file handle."""
        if self.fd:
            try:
                self.fd.flush()
                self.fd.close()
            except Exception:
                pass
            self.fd = None

    def __del__(self):
        """Clean up file handle on deletion."""
        self.close()

    def initSender(self):
        """Open file or pipe for writing.

        Creates directory structure if needed.
        Opens pipes in write mode, files in append mode.
        """
        directory = os.path.dirname(self.file_name)
        if not os.path.exists(directory) and len(directory) > 0:
            os.makedirs(directory)

        if self.is_pipe:
            self.fd = open(self.file_name, "w")
        else:
            self.fd = open(self.file_name, "a")

    def sendMessages(self, msg):
        """Send multiple messages and return expected results.

        Overrides parent to maintain compatibility with original implementation.

        Args:
            msg: Base message text

        Returns:
            List of tuples for validation
        """
        return super(FileSender, self).sendMessages(msg)

    def sendMessage(self, msg):
        """Write single message with padding and termination.

        Args:
            msg: Formatted message string to write
        """
        line = '%s%s' % (msg, self.terminate_seq)

        # Add null padding if specified
        if self.padding:
            line += '\0' * (self.padding - len(line))

        # Write byte-by-byte or all at once
        if self.send_by_bytes:
            for c in line:
                self.fd.write(c)
                self.fd.flush()
        else:
            self.fd.write(line)
            self.fd.flush()

    def __str__(self):
        """Return string representation of sender type."""
        if self.is_pipe:
            if self.padding:
                return 'pipe(%s[%d])' % (self.file_name, self.padding)
            else:
                return 'pipe(%s)' % (self.file_name,)
        else:
            return 'file(%s)' % (self.file_name,)
