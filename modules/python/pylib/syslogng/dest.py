#############################################################################
# Copyright (c) 2022 Balazs Scheidler <bazsi77@gmail.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
# As an additional exemption you are allowed to compile & link against the
# OpenSSL libraries as published by the OpenSSL project. See the file
# COPYING for details.
#
#############################################################################
try:
    from _syslogng import LogDestination

except ImportError:
    import warnings
    warnings.warn("You have imported the syslogng package outside of syslog-ng, thus some of the functionality is not available. Defining fake classes for those exported by the underlying syslog-ng code")

    LogDestination = object

class LogDestination(LogDestination):
    LTR_DROP = 0
    LTR_ERROR = 1
    LTR_EXPLICIT_ACK_MGMT = 2
    LTR_SUCCESS = 3
    LTR_QUEUED = 4
    LTR_NOT_CONNECTED = 5
    LTR_RETRY = 6

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
        """Initialize this log element

        This method is called when the syslog-ng configuration is
        initialized.  You can prevent the configuration to be accepted by
        returning False, in which case you should log why that was the case.

        The same object can be initalized multiple times in case the
        configuration reload operation fails in syslog-ng:
          1) init() when the config is first initialized
          2) deinit() as a preparation for reload
          3) initialization of the new config fails for some reason (any one
             object can cause it to fail)
          4) init() again, as syslog-ng reverts to the old config in such cases.

        Args:
            options:
                is a dictionary that contains options passed using the
                options() parameter of the python() destination

        Returns:
            bool: True if successful, False otherwise
        """
        return True

    def deinit(self):
        """Deinitialize this log element

        This method is called when the syslog-ng configuration is being shut
        down.
        """
        pass

    def send(self, msg):
        """Send a message to the target service

        This method is invoked by the underlying C code to dispatch a
        message to the target service.

        Args:
            msg (LogMessage): message to be sent

        Returns:
        """
        return self.LTR_SUCCESS
