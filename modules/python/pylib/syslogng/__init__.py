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

from __future__ import print_function

try:
    from _syslogng import LogMessage
    from _syslogng import LogSource, LogFetcher
    from _syslogng import LogTemplate, LogTemplateException, LTZ_LOCAL, LTZ_SEND
    from _syslogng import Logger
    from _syslogng import Persist as SlngPersist
    from _syslogng import InstantAckTracker, ConsecutiveAckTracker, BatchedAckTracker

    class Persist(SlngPersist):
        def __init__(self, persist_name, defaults=None):
            super(Persist, self).__init__(persist_name)

            if defaults:
                for key, value in defaults.items():
                    if key not in self:
                        self[key] = value

except ImportError:
    print("The syslogng package can only be used in syslog-ng.")
