#############################################################################
# Copyright (c) 2025 Narkhov Evgeny <evgenynarkhov2@gmail.com>
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
from typing import Dict
from typing import Optional

try:
    from syslogng import LogParser, LogMessage
except ImportError:
    raise RuntimeError(
        "This code was designed to run with syslog-ng runtime python bindings. "
        "You probably imported it without syslog-ng installed!",
    )


class CopyParser(LogParser):
    """
    This python parser copies MSG field into a new one,
    specified by `__target_field` option. It also drops
    messages that start with `__drop_prefix`.
    """

    __target_field_option = "target_field"
    __drop_prefix_option = "drop_prefix"

    def __init__(self):
        self.__logger = logging.getLogger("copy-parser")
        self.__target_field: Optional[str] = None
        self.__drop_prefix: Optional[str] = None

    def __error(self, message: str, *args: str) -> bool:
        self.__logger.error(message, *args)
        return False

    def __missing_option(self, option: str, options: Dict[str, str]) -> bool:
        return self.__error("could not find %s key in options: %s", option, ', '.join(options))

    def init(self, options: Dict[str, str]) -> bool:
        if self.__target_field_option not in options:
            return self.__missing_option(self.__target_field_option, options)

        if self.__drop_prefix_option not in options:
            return self.__missing_option(self.__drop_prefix_option, options)

        self.__target_field = options[self.__target_field_option]
        self.__drop_prefix = options[self.__drop_prefix_option]
        return True

    def deinit(self) -> bool:
        return True

    def parse(self, msg: LogMessage) -> bool:
        if any(f is None for f in (self.__target_field, self.__drop_prefix)):
            return self.__error("some required fields were not initialized. Probably init() did not run?")

        if b"MESSAGE" not in msg.keys():
            return self.__error("failed to get MESSAGE field from message instance: key not found")

        if msg["MESSAGE"].startswith(self.__drop_prefix.encode()):
            return False

        msg[self.__target_field] = msg["MESSAGE"]
        return True
