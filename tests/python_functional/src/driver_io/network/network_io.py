#!/usr/bin/env python
#############################################################################
# Copyright (c) 2020 One Identity
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
from enum import Enum

from pathlib2 import Path

import src.testcase_parameters.testcase_parameters as tc_parameters
from src.common.file import File
from src.common.random_id import get_unique_id
from src.helpers.loggen.loggen import Loggen


class NetworkIO():
    def __init__(self, ip, port, transport):
        self.__ip = ip
        self.__port = port
        self.__transport = transport

    def write(self, content, rate=None):
        loggen_input_file_path = Path(tc_parameters.WORKING_DIR, "loggen_input_{}.txt".format(get_unique_id()))

        loggen_input_file = File(loggen_input_file_path)
        loggen_input_file.open(mode="w")
        loggen_input_file.write(content)
        loggen_input_file.close()

        Loggen().start(self.__ip, self.__port, read_file=str(loggen_input_file_path), dont_parse=True, permanent=True, rate=rate, **self.__transport.value)

    class Transport(Enum):
        TCP = {"inet": True, "stream": True}
        UDP = {"dgram": True}
        TLS = {"use_ssl": True}
        PROXIED_TCP = {"inet": True, "stream": True}
        PROXIED_TLS = {"use_ssl": True}
        PROXIED_TLS_PASSTHROUGH = {"use_ssl": True, "proxied_tls_passthrough": True}
