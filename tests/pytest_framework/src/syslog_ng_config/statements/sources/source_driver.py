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


class SourceDriver(object):
    def __init__(self, logger_factory, IOClass):
        self.__logger_factory = logger_factory
        self.__logger = logger_factory.create_logger("SourceDriver")
        self.__IOClass = IOClass
        self.__writer = None

    def __construct_writer(self, path):
        if not self.__writer:
            self.__writer = self.__IOClass(self.__logger_factory, path)

    def sd_write_log(self, path, formatted_log, counter):
        self.__construct_writer(path)
        for __i in range(0, counter):
            self.__writer.write(formatted_log)
        self.__logger.print_io_content(
            path, formatted_log, "Content has been written to number of times: {}".format(counter)
        )
