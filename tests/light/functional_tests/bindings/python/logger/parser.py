#############################################################################
# Copyright (c) 2025 One Identity
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

try:
    from syslogng import LogParser, LogMessage, TRACE
except ImportError:
    raise RuntimeError(
        "This code was designed to run with syslog-ng runtime python bindings. "
        "You probably imported it without syslog-ng installed!",
    )


class ParserThatLogsToInternal(LogParser):
    """ This python parser just logs messages to internal() source. """

    def __init__(self):
        self.__logger = logging.getLogger("parser-that-logs-to-internal")

    def init(self, _options: Dict[str, str]) -> bool:
        return True

    def deinit(self) -> bool:
        return True

    def parse(self, msg: LogMessage) -> bool:
        if b"MESSAGE" in msg.keys():
            for log_level in (TRACE, logging.DEBUG, logging.INFO, logging.WARNING, logging.ERROR):
                self.__logger.log(log_level, "hey, this is parser-that-logs-to-internal, here is the message: %s", msg["MESSAGE"].decode())

            # syslog-ng bound logging should provide this additional method.
            self.__logger.trace("hey again, here is the bound trace method of internal logger!")
            # Logs should be filtered by effective level, that is set by syslogng package.
            self.__logger.info("the log level is %s" % self.__logger.getEffectiveLevel())

        return True
