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

import pytest
from src.logger.logger_factory import LoggerFactory


def test_logger_factory(tc_unittest):
    temp_file = tc_unittest.get_temp_file()
    with pytest.raises(KeyError):
        LoggerFactory(temp_file, loglevel="random-loglevel")


def test_create_logger(tc_unittest):
    temp_file = tc_unittest.get_temp_file()
    logger_factory = LoggerFactory(temp_file, loglevel="info")
    logger_factory.create_logger("UnitTest")
