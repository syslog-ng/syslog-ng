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

from src.common.blocking import wait_until_true
from src.common.operations import open_file

logger = logging.getLogger(__name__)


class File(object):
    def __init__(self, file_path):
        self.__file_path = file_path
        self.__opened_file = None

    def __del__(self):
        if self.__opened_file:
            self.__opened_file.close()
            self.__opened_file = None

    def __is_file_exist(self):
        return self.__file_path.exists()

    def wait_for_creation(self):
        file_created = wait_until_true(self.__is_file_exist)
        if file_created:
            logger.debug("File has been created:\n{}".format(self.__file_path))
        else:
            logger.debug("File not created:\n{}".format(self.__file_path))
        return file_created

    def open_file(self, mode):
        self.__opened_file = open_file(self.__file_path, mode)
        return self.__opened_file
