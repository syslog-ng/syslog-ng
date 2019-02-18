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
from src.logger.logger import Logger


def test_logger_info_level(tc_unittest):
    temp_file = tc_unittest.get_temp_file()
    logger = Logger("UnitTest", temp_file, logging.INFO)
    logger.info("InfoMessage")
    logger.debug("DebugMessage")
    logger.error("ErrorMessage")

    assert " - UnitTest - INFO - InfoMessage" in temp_file.read_text()
    assert " - UnitTest - ERROR - ErrorMessage" in temp_file.read_text()
    assert " - UnitTest - DEBUG - DebugMessage" not in temp_file.read_text()


def test_logger_debug_level(tc_unittest):
    temp_file = tc_unittest.get_temp_file()
    logger = Logger("UnitTest", temp_file, logging.DEBUG)
    logger.info("InfoMessage")
    logger.debug("DebugMessage")
    logger.error("ErrorMessage")

    assert " - UnitTest - INFO - InfoMessage" in temp_file.read_text()
    assert " - UnitTest - ERROR - ErrorMessage" in temp_file.read_text()
    assert " - UnitTest - DEBUG - DebugMessage" in temp_file.read_text()


def test_logger_error_level(tc_unittest):
    temp_file = tc_unittest.get_temp_file()
    logger = Logger("UnitTest", temp_file, logging.ERROR)
    logger.info("InfoMessage")
    logger.debug("DebugMessage")
    logger.error("ErrorMessage")

    assert " - UnitTest - INFO - InfoMessage" not in temp_file.read_text()
    assert " - UnitTest - ERROR - ErrorMessage" in temp_file.read_text()
    assert " - UnitTest - DEBUG - DebugMessage" not in temp_file.read_text()


def test_logger_file_handler_disabled(tc_unittest):
    temp_file = tc_unittest.get_temp_file()
    logger = Logger("UnitTest", temp_file, logging.DEBUG, use_console_handler=True, use_file_handler=False)
    logger.info("TestMessage")
    assert temp_file.exists() is False
