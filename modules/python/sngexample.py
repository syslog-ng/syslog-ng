#############################################################################
# Copyright (c) 2015-2016 Balabit
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

class LogDestination(object):

    def open(self):
        """Open a connection to the target service"""
        return True

    def close(self):
        """Close the connection to the target service"""
        pass

    def is_opened(self):
        """Check if the connection to the target is able to receive messages"""
        return True

    def init(self, options):
        """This method is called at initialization time"""
        return True

    def deinit(self):
        """This method is called at deinitialization time"""
        pass

    def send(self, msg):
        """Send a message to the target service

        It can return either a boolean or integer.
        Boolean: True to indicate success, False will suspend the
        destination for a period specified by the time-reopen() option.
        After that the same message is retried until retries() times.

        Integer:
        self.SUCCESS: message sending was successful (same as boolean True)
        self.ERROR: message sending was unsuccessful. Same message is retried.
            (same as boolean False)
        self.DROP: message cannot be sent, it should be dropped immediately.
        self.QUEUED: message is not sent immediately, it will be sent with the flush method.
        self.NOT_CONNECTED: message is put back to the queue, open method will be called until success.
        self.RETRY: message is put back to the queue, try to send again until 3 times, then fallback to self.NOT_CONNECTED."""

        pass

    def flush(self):
        """Flush the queued messages

        It can return either a boolean or integer.
        Boolean: True to indicate that the batch is successfully sent.
        False indicates error while sending the batch. The destination is suspended
        for time-reopen period. The messages in the batch are passed again to send, one by one.

        Integer:
        self.SUCCESS: batch sending was successful (same as boolean True)
        self.ERROR: batch sending was unsuccessful. (same as boolean False)
        self.DROP: batch cannot be sent, the messages should be dropped immediately.
        self.NOT_CONNECTED: the messages in the batch is put back to the queue,
            open method will be called until success.
        self.RETRY: message is put back to the queue, try to send again until 3 times, then fallback to self.NOT_CONNECTED."""

        pass

class DummyPythonDest(object):
    def send(self, msg):
        print('queue', msg)
        return self.SUCCESS

class DummyBatchDestination(object):
    def init(self, options):
        self.bulk = list()
        return True

    def send(self, msg):
         self.bulk.append(msg["MSG"].decode())
         return self.QUEUED

    def flush(self):
        print("flushing: " + ",".join(self.bulk))
        self.bulk = list()
        return self.SUCCESS
