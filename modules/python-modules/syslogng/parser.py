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
# pylint: disable=function-redefined

try:
    from _syslogng import LogParser

except ImportError:
    import warnings
    warnings.warn("You have imported the syslogng package outside of syslog-ng, "
                  "thus some of the functionality is not available. "
                  "Defining fake classes for those exported by the underlying syslog-ng code")

    LogParser = object


class LogParser(LogParser):
    def init(self, options):
        """This method is called when the configuration is initialized

        Args:
            options:
                is a dictionary that contains options passed using the
                options() parameter of the python() destination

        Returns:
            bool: True if successful, False otherwise
        """
        return True

    def deinit(self):
        """This method is called when the configuration is deinitialized"""

    def parse(self, msg):
        """This method is called to 'parse' the message

        Args:
            msg: LogMessage
                is a syslog-ng message that allows dict-like access to
                name-value pairs.

        Returns:
            bool: True if parsing was successful, False to drop the message
        """
        raise NotImplementedError
