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
# pylint: disable=unused-import

try:
    from _syslogng import LogMessage

except ImportError:
    import warnings
    warnings.warn("You have imported the syslogng package outside of syslog-ng, "
                  "thus some of the functionality is not available. "
                  "Defining fake classes for those exported by the underlying syslog-ng code")

    class LogMessage(dict):

        # syslog-ng internally stores everything as a sequence of bytes that
        # is strongly "preferred" to be an utf8 string.  It takes a bytes
        # and stores it verbatim.  Strings are converted to utf8 (not the
        # local character set!)

        def __init__(self, msg):
            super().__init__()
            self["MESSAGE"] = msg
            self.bookmark = None

        def __getitem__(self, key):
            if isinstance(key, str):
                key = key.encode('utf8')
            return super().__getitem__(key)

        def __setitem__(self, key, value):
            if isinstance(key, str):
                key = key.encode('utf8')
            if isinstance(value, str):
                value = value.encode('utf8')
            super().__setitem__(key, value)

        def __eq__(self, other):
            return False

        def set_bookmark(self, bookmark):
            self.bookmark = bookmark
