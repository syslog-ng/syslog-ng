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
import logging

from src.syslog_ng_config.statements.global_driver import GlobalDriver

logger = logging.getLogger(__name__)


class SourceDriver(GlobalDriver):
    group_type = "source"

    def __init__(self, driver_name, driver_resources, options, positional_option=None, connection_options=None):
        super(SourceDriver, self).__init__(driver_name, driver_resources, options, positional_option, connection_options)
        self.writer = None

    def construct_writer(self):
        assert self.driver_io
        if not self.writer:
            self.writer = self.driver_io(self.connection_values)

    def write_log(self, formatted_log, counter=1):
        self.calculate_connection_value()
        self.construct_writer()
        for __i in range(0, counter):
            self.writer.write(formatted_log)
        written_description = "Content has been written to\nresource: {}\nnumber of times: {}\ncontent: {}\n".format(
            self.connection_values, counter, formatted_log
        )
        logger.info(written_description)
