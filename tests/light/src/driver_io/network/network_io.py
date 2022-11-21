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
import socket
from enum import Enum
from enum import IntEnum
from pathlib import Path

from src.common.asynchronous import BackgroundEventLoop
from src.common.blocking import DEFAULT_TIMEOUT
from src.common.file import File
from src.common.network import SingleConnectionTCPServer
from src.common.network import UDPServer
from src.common.random_id import get_unique_id
from src.driver_io import message_readers
from src.helpers.loggen.loggen import Loggen


class NetworkIO():
    def __init__(self, ip, port, transport, ip_proto_version=None):
        self.__ip = ip
        self.__port = port
        self.__transport = transport
        self.__ip_proto_version = NetworkIO.IPProtoVersion.V4 if ip_proto_version is None else ip_proto_version
        self.__server = None
        self.__message_reader = None

    def write(self, content, rate=None):
        loggen_input_file_path = Path("loggen_input_{}.txt".format(get_unique_id()))

        loggen_input_file = File(loggen_input_file_path)
        loggen_input_file.write_content_and_close(content)

        Loggen().start(self.__ip, self.__port, read_file=str(loggen_input_file_path), dont_parse=True, permanent=True, rate=rate, **self.__transport.value)

    def start_listener(self):
        self.__message_reader, self.__server = self.__transport.construct(self.__port, self.__ip, self.__ip_proto_version)
        BackgroundEventLoop().wait_async_result(self.__server.start(), timeout=DEFAULT_TIMEOUT)

    def stop_listener(self):
        if self.__message_reader is not None:
            BackgroundEventLoop().wait_async_result(self.__server.stop(), timeout=DEFAULT_TIMEOUT)
            self.__message_reader = None
            self.__server = None

    def read_number_of_messages(self, counter, timeout=DEFAULT_TIMEOUT):
        return self.__message_reader.wait_for_number_of_messages(counter, timeout)

    def read_until_messages(self, lines, timeout=DEFAULT_TIMEOUT):
        return self.__message_reader.wait_for_messages(lines, timeout)

    class IPProtoVersion(IntEnum):
        V4 = socket.AF_INET
        V6 = socket.AF_INET6

    class Transport(Enum):
        TCP = {"inet": True, "stream": True}
        UDP = {"dgram": True}
        TLS = {"use_ssl": True}
        PROXIED_TCP = {"inet": True, "stream": True}
        PROXIED_TLS = {"use_ssl": True}
        PROXIED_TLS_PASSTHROUGH = {"use_ssl": True, "proxied_tls_passthrough": True}

        def construct(self, port, host=None, ip_proto_version=None):
            def _construct(server, reader_class):
                return reader_class(server), server

            transport_mapping = {
                NetworkIO.Transport.TCP: lambda: _construct(SingleConnectionTCPServer(port, host, ip_proto_version), message_readers.SingleLineStreamReader),
                NetworkIO.Transport.UDP: lambda: _construct(UDPServer(port, host, ip_proto_version), message_readers.DatagramReader),
            }
            return transport_mapping[self]()
