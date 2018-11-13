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

import sys
import logging
from colorlog import ColoredFormatter


class Logger(logging.Logger):
    def __init__(self, logger_name, report_file, loglevel, use_console_handler=True, use_file_handler=True):
        super(Logger, self).__init__(logger_name, loglevel)
        self.handlers = []
        if use_console_handler:
            self.__set_console_handler()
        if use_file_handler:
            self.__set_file_handler(file_path=report_file)

    def __set_file_handler(self, file_path=None):
        # FileHandler can work only with string representation of file_path
        file_handler = logging.FileHandler(str(file_path))
        formatter = logging.Formatter("%(asctime)s - %(name)s - %(levelname)s - %(message)s")
        file_handler.setFormatter(formatter)
        self.addHandler(file_handler)

    def __set_console_handler(self):
        console_handler = logging.StreamHandler(sys.stdout)
        logging.captureWarnings(capture=True)
        formatter = ColoredFormatter(
            "\n%(log_color)s%(asctime)s - %(name)s - %(levelname)-5s%(reset)s- %(message_log_color)s%(message)s",
            datefmt=None,
            reset=True,
            log_colors={
                "DEBUG": "cyan",
                "INFO": "green",
                "WARNING": "yellow",
                "ERROR": "red",
                "CRITICAL": "red,bg_white",
            },
            secondary_log_colors={"message": {"ERROR": "red", "CRITICAL": "red"}},
            style="%",
        )
        console_handler.setFormatter(formatter)
        self.addHandler(console_handler)

    def print_io_content(self, path, content, main_message):
        message = "{}\
        \n->Path:[{}]\
        \n->Content:[{}]".format(
            main_message, path, content
        )
        self.log(logging.INFO, message)
