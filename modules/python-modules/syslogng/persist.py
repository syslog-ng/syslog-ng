#############################################################################
# Copyright (c) 2022 Balazs Scheidler <bazsi77@gmail.com>
# Copyright (c) 2015-2016 Balabit
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
# pylint: disable=unused-import,function-redefined

try:
    from _syslogng import Persist

except ImportError:
    import warnings
    warnings.warn("You have imported the syslogng package outside of syslog-ng, "
                  "thus some of the functionality is not available. "
                  "Defining fake classes for those exported by the underlying syslog-ng code")

    # fake Persist class
    class Persist(dict):
        def __init__(self, persist_name):
            self.persist_name = persist_name


class Persist(Persist):
    def __init__(self, persist_name, defaults=None):
        super().__init__(persist_name)

        if defaults:
            for key, value in defaults.items():
                if key not in self:
                    self[key] = value
