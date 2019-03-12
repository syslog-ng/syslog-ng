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


class SourceDriver(object):
    group_type = "source"

    def __init__(self, IOClass, positional_parameters=[], options={}):
        self.__IOClass = IOClass
        self.__writer = None
        self.positional_parameters = positional_parameters
        self.options = options

    def __construct_writer(self, path):
        if not self.__writer:
            self.__writer = self.__IOClass(path)

    def sd_write_log(self, path, formatted_log, counter):
        self.__construct_writer(path)
        for __i in range(0, counter):
            self.__writer.write(formatted_log)
        written_description = "Content has been written to\nresource: {}\nnumber of times: {}\ncontent: {}\n".format(path, counter, formatted_log)
        logger.info(written_description)
