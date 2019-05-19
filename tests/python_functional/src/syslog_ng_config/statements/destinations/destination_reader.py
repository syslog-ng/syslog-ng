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

logger = logging.getLogger(__name__)


class DestinationReader(object):
    def __init__(self, driver_io_cls, driver_io_parser_cls):
        self.driver_io_cls = driver_io_cls
        self.driver_io_parser_cls = driver_io_parser_cls
        self.actual_driver_resource = None
        self.driver_io = None
        self.reader = None

    def construct_reader(self, driver_resource):
        if not self.driver_io or (self.actual_driver_resource != driver_resource):
            self.actual_driver_resource = driver_resource
            self.driver_io = self.driver_io_cls(driver_resource)
            self.driver_io.wait_for_creation()
            self.reader = MessageReader(self.driver_io.read, self.driver_io_parser_cls())

    def read_logs(self, driver_resource, counter=1):
        self.construct_reader(driver_resource)
        messages = self.reader.pop_messages(counter)
        read_description = "Content has been read from\nresource: {}\ncontent: {}\n".format(driver_resource, messages)
        logger.info(read_description)
        return messages
