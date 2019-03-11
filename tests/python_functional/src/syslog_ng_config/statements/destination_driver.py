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

from src.message_reader.message_reader import MessageReader
from src.syslog_ng_config.statements.global_driver import GlobalDriver

logger = logging.getLogger(__name__)


class DestinationDriver(GlobalDriver):
    group_type = "destination"

    def __init__(self, driver_name, driver_resources, options, positional_option=None, connection_options=None):
        super(DestinationDriver, self).__init__(driver_name, driver_resources, options, positional_option, connection_options)
        self.reader = None

    def construct_reader(self):
        assert self.driver_io
        if not self.reader:
            driver_io = self.driver_io(self.connection_values)
            driver_io.wait_for_creation()
            self.reader = MessageReader(driver_io.read, self.driver_parser())

    def read_log(self):
        return self.read_logs(counter=1)[0]

    def read_logs(self, counter):
        self.calculate_connection_value()
        self.construct_reader()
        messages = self.reader.pop_messages(counter)
        read_description = "Content has been read from\nresource: {}\ncontent: {}\n".format(self.connection_values, messages)
        logger.info(read_description)
        return messages
