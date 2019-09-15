#!/usr/bin/env python
#############################################################################
# Copyright (c) 2015-2019 Balabit
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
from src.syslog_ng_config.statements.option_handlers.basic import BasicOptionHandler
from src.syslog_ng_config.statements.sources.source_driver import SourceDriver


class ExampleMsgGeneratorSource(SourceDriver):
    def __init__(self, **options):
        self.driver_name = "example_msg_generator"
        self.DEFAULT_MESSAGE = "-- Generated message. --"
        self.options = options

        self.__basic_option_handler = BasicOptionHandler(options=self.options)
        super(ExampleMsgGeneratorSource, self).__init__(self.__basic_option_handler)
