#############################################################################
# Copyright (c) 2022 Balazs Scheidler
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

#
# This is a sample Python destination module that show-cases how to
# implement Python modules and Python based destinations, see
# modules/python-modules/README.md for more details
#

from syslogng import LogDestination
import logging


class ExampleDestination(LogDestination):

    logger = logging.getLogger(__name__)

    def __init__(self):
        self._is_opened = False

    def init(self, options):
        self.logger.info("ExampleDestination: initialized with {}".format(options))
        return True

    def is_opened(self):
        return self._is_opened

    def open(self):
        self.logger.info("ExampleDestination: opened")
        self._is_opened = True
        return True

    def close(self):
        self.logger.info("ExampleDestination: closed")
        self._is_opened = False

    def deinit(self):
        self.logger.info("ExampleDestination: deinit")

    def send(self, msg):
        self.logger.info("ExampleDestination: Name Value Pairs are:")

        for key in msg.keys():
            value = msg[key]
            self.logger.info("ExampleDestination: {} = {} {}".format(key, value, type(value)))
        return True
