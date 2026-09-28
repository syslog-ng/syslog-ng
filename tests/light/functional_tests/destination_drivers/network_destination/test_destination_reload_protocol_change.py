#!/usr/bin/env python
#############################################################################
# Copyright (c) 2026 One Identity
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
import queue
import re
import socket
import ssl
import threading

import pytest

from src.common.file import copy_shared_file


class DestinationListener:
    def __init__(self, port, transport, testcase_parameters):
        self.transport = transport
        self.is_udp = transport == "udp"
        self.is_tls = "tls" in transport
        self.messages = queue.Queue()
        self.stopping = threading.Event()
        self.connections = []
        self.listener = socket.socket(socket.AF_INET, socket.SOCK_DGRAM if self.is_udp else socket.SOCK_STREAM)
        self.listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.listener.bind(("127.0.0.1", port))

        if self.is_udp:
            self.listener.settimeout(0.2)
            self.ssl_context = None
        else:
            self.listener.listen()
            self.listener.settimeout(0.2)
            self.ssl_context = None
            if self.is_tls:
                self.ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
                self.ssl_context.load_cert_chain(
                    copy_shared_file(testcase_parameters, "server.crt"),
                    copy_shared_file(testcase_parameters, "server.key"),
                )

        self.thread = threading.Thread(target=self._run, daemon=True)
        self.thread.start()

    def close(self):
        self.stopping.set()
        self.listener.close()
        for connection in self.connections:
            try:
                connection.close()
            except OSError:
                pass
        self.thread.join(timeout=2)

    def read_message(self):
        return self.messages.get(timeout=15)

    def _run(self):
        if self.is_udp:
            self._receive_datagrams()
            return

        while not self.stopping.is_set():
            try:
                connection, _ = self.listener.accept()
            except (OSError, TimeoutError):
                continue
            self.connections.append(connection)
            self._receive_stream(connection)

    def _receive_datagrams(self):
        while not self.stopping.is_set():
            try:
                message, _ = self.listener.recvfrom(65535)
            except (OSError, TimeoutError):
                continue
            self.messages.put(message.decode("utf-8", errors="replace"))

    def _receive_stream(self, connection):
        try:
            if self.is_tls:
                connection = self.ssl_context.wrap_socket(connection, server_side=True)
                self.connections.append(connection)

            pending = bytearray()
            while not self.stopping.is_set():
                chunk = connection.recv(4096)
                if not chunk:
                    return
                pending.extend(chunk)
                self._extract_messages(pending)
        except (OSError, ssl.SSLError):
            return

    def _extract_messages(self, pending):
        while pending:
            if pending.startswith(b"PROXY "):
                header_end = pending.find(b"\r\n")
                if header_end < 0:
                    return
                del pending[:header_end + 2]
                continue

            space = pending.find(b" ")
            if space > 0 and pending[:space].isdigit():
                payload_size = int(pending[:space])
                payload_start = space + 1
                if len(pending) < payload_start + payload_size:
                    return
                message = bytes(pending[payload_start:payload_start + payload_size])
                del pending[:payload_start + payload_size]
                self.messages.put(message.decode("utf-8", errors="replace"))
                continue

            newline = pending.find(b"\n")
            if newline < 0:
                return
            message = bytes(pending[:newline + 1])
            del pending[:newline + 1]
            self.messages.put(message.decode("utf-8", errors="replace"))


@pytest.mark.parametrize(
    ("old_driver", "new_driver", "transport"),
    [
        ("network", "syslog", "tcp"),
        ("syslog", "network", "tcp"),
        ("network", "syslog", "tls"),
        ("syslog", "network", "tls"),
        ("network", "syslog", "udp"),
        ("syslog", "network", "udp"),
    ],
)
def test_protocol_change_on_destination_reload(
    config, syslog_ng, port_allocator, testcase_parameters,
    old_driver, new_driver, transport,
):
    port = port_allocator()
    listener = DestinationListener(port, transport, testcase_parameters)
    source = config.create_example_msg_generator_source(
        num=1,
        freq=0,
        template=config.stringify("before-reload"),
    )
    destination = config.create_network_destination(ip="localhost", port=port, transport="udp")
    destination.driver_name = old_driver
    destination.options["transport"] = transport
    if "tls" in transport:
        destination.options["tls"] = {"peer-verify": "none"}
    logpath = config.create_logpath(statements=[source, destination])

    try:
        syslog_ng.start(config)
        assert "before-reload" in listener.read_message()

        # Recreate the generator so the post-reload wire format is unambiguous.
        new_source = config.create_example_msg_generator_source(
            num=1,
            freq=0,
            template=config.stringify("after-reload"),
        )
        logpath.logpath[0][0] = new_source
        destination.driver_name = new_driver
        syslog_ng.reload(config)

        output = listener.read_message()
        is_rfc5424 = re.match(r"<\d+>1 ", output) is not None
        assert is_rfc5424 == (new_driver == "syslog")
        assert "after-reload" in output
    finally:
        listener.close()
