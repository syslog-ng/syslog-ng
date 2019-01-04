#!/usr/bin/env python
#############################################################################
# Copyright (c) 2015-2018 Balabit
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


class SyslogNg(object):
    def __init__(self, syslog_ng_cli):
        self.__syslog_ng_cli = syslog_ng_cli

    def start(self, config):
        self.__syslog_ng_cli.start(config)

    def stop(self, unexpected_messages=None):
        self.__syslog_ng_cli.stop(unexpected_messages)

    def reload(self, config):
        self.__syslog_ng_cli.reload(config)

    def restart(self, config):
        self.__syslog_ng_cli.start(config)
        self.__syslog_ng_cli.stop()

    def get_version(self):
        return self.__syslog_ng_cli.get_version()
