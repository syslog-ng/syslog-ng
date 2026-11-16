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
# Constants from original implementation
SYSLOG_PREFIX = "2004-09-07T10:43:21+01:00 bzorp prog[12345]:"
SYSLOG_NEW_PREFIX = "2004-09-07T10:43:21+01:00 bzorp prog 12345 - -"
PADDING = 'x' * 250

# Global session counter
_session_counter = 0


def get_session_counter():
    """Get current session counter value."""
    return _session_counter


def increment_session_counter():
    """Increment and return session counter."""
    global _session_counter
    _session_counter += 1
    return _session_counter


def reset_session_counter():
    """Reset session counter to 0."""
    global _session_counter
    _session_counter = 0


class MessageSender(object):
    """Base class for sending messages to syslog-ng sources.

    This class provides the framework for generating and sending formatted
    syslog messages with session tracking and validation support.
    """

    def __init__(self, repeat=100, new_protocol=0, dgram=0):
        """Initialize message sender.

        Args:
            repeat: Number of messages to send
            new_protocol: Use RFC5424 format (1) or RFC3164 format (0)
            dgram: Datagram mode flag (used by subclasses)
        """
        self.repeat = repeat
        self.new_protocol = new_protocol
        self.dgram = dgram

    def close(self):
        """Release sender resources. Subclasses override if cleanup is needed."""
        pass

    def sendMessages(self, msg, pri=7):
        """Send multiple formatted messages.

        Args:
            msg: Base message text
            pri: Syslog priority (default: 7 = debug)

        Returns:
            List of tuples: [(msg, session_counter, repeat), ...]
            Used for validation in tests.
        """
        session = get_session_counter()

        expected = []
        try:
            self.initSender()
            for counter in range(1, self.repeat):
                if self.new_protocol == 0:
                    # RFC3164 format
                    line = '<%d>%s %s %03d/%05d %s %s' % (
                        pri,
                        SYSLOG_PREFIX,
                        msg,
                        session,
                        counter,
                        str(self),
                        PADDING,
                    )
                else:
                    # RFC5424 format
                    line = '<%d>1 %s %s %03d/%05d %s %s' % (
                        pri,
                        SYSLOG_NEW_PREFIX,
                        msg,
                        session,
                        counter,
                        str(self),
                        PADDING,
                    )

                # Add framing for TCP with new protocol
                if self.dgram == 0 and self.new_protocol == 1:
                    line = '%d %s' % (len(line), line)

                self.sendMessage(line)
        finally:
            self.close()

        expected.append((msg, session, self.repeat))
        increment_session_counter()
        return expected

    def initSender(self):
        """Initialize the sender (open connections, files, etc.).

        Must be implemented by subclasses.
        """
        raise NotImplementedError("Subclasses must implement initSender()")

    def sendMessage(self, msg):
        """Send a single formatted message.

        Must be implemented by subclasses.

        Args:
            msg: Formatted message string to send
        """
        raise NotImplementedError("Subclasses must implement sendMessage()")

    def __str__(self):
        """Return string representation of sender type.

        Must be implemented by subclasses.
        """
        raise NotImplementedError("Subclasses must implement __str__()")
