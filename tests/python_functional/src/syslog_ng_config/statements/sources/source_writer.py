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
logger = logging.getLogger(__name__)


class SourceWriter(object):
    def __init__(self, driver_io_cls):
        self.driver_io_cls = driver_io_cls
        self.actual_driver_resource = None
        self.driver_io = None

    def construct_writer(self, driver_resource):
        if not self.driver_io or (self.actual_driver_resource != driver_resource):
            self.actual_driver_resource = driver_resource
            self.driver_io = self.driver_io_cls(driver_resource)

    def write_log(self, driver_resource, formatted_log, counter=1):
        self.construct_writer(driver_resource)
        for __i in range(0, counter):
            self.driver_io.write(formatted_log)
        written_description = "Content has been written to\nresource: {}\nnumber of times: {}\ncontent: {}\n".format(driver_resource, counter, formatted_log)
        logger.info(written_description)
