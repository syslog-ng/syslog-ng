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
import socket
import ssl

import pytest

from src.common.file import copy_shared_file


@pytest.mark.parametrize(
    ("old_driver", "new_driver", "transport"),
    [
        ("network", "syslog", "tcp"),
        ("syslog", "network", "tcp"),
        ("network", "syslog", "tls"),
        ("syslog", "network", "tls"),
        ("network", "syslog", "proxied-tcp"),
        ("syslog", "network", "proxied-tcp"),
        ("network", "syslog", "proxied-tls"),
        ("syslog", "network", "proxied-tls"),
        ("network", "syslog", "proxied-tls-passthrough"),
        ("syslog", "network", "proxied-tls-passthrough"),
        ("network", "syslog", "udp"),
        ("syslog", "network", "udp"),
        ("network", "syslog", "auto"),
        ("syslog", "network", "auto"),
    ],
)
def test_protocol_change_on_source_reload(
    config, syslog_ng, port_allocator, testcase_parameters,
    old_driver, new_driver, transport,
):
    port = port_allocator()
    source_options = {"ip": "localhost", "port": port, "transport": f'"{transport}"'}
    uses_tls = "tls" in transport
    uses_proxy_protocol = transport.startswith("proxied-")
    if uses_tls:
        source_options["tls"] = {
            "key-file": copy_shared_file(testcase_parameters, "server.key"),
            "cert-file": copy_shared_file(testcase_parameters, "server.crt"),
            "peer-verify": '"optional-untrusted"',
        }

    old_source = getattr(config, f"create_{old_driver}_source")(**source_options)
    destination = config.create_file_destination(file_name="output.log", template=r'"${MESSAGE}\n"')
    logpath = config.create_logpath(statements=[old_source, destination])
    syslog_ng.start(config)

    def open_client():
        client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client.connect(("127.0.0.1", port))
        if uses_proxy_protocol and transport != "proxied-tls":
            client.sendall(f"PROXY TCP4 127.0.0.1 127.0.0.1 10000 {port}\r\n".encode("ascii"))
        if uses_tls:
            client = ssl._create_unverified_context().wrap_socket(client, server_hostname="localhost")
        if transport == "proxied-tls":
            client.sendall(f"PROXY TCP4 127.0.0.1 127.0.0.1 10000 {port}\r\n".encode("ascii"))
        return client

    def send_message(client, driver, text):
        if driver == "network":
            message = f"<34>Sep 28 10:00:00 host app: {text}\n".encode("ascii")
        else:
            message = f"<34>1 2026-09-28T12:00:00Z host app 123 ID47 - {text}".encode("ascii")

        if transport == "udp":
            client.sendto(message, ("127.0.0.1", port))
        elif driver == "network":
            client.sendall(message)
        else:
            payload = message + b"\n"
            client.sendall(str(len(payload)).encode("ascii") + b" " + payload)

    if transport == "udp":
        old_client = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    else:
        old_client = open_client()

    try:
        # Confirm the old protocol is active before reloading.
        send_message(old_client, old_driver, "before-reload")
        assert destination.read_log() == "before-reload\n"

        new_source = getattr(config, f"create_{new_driver}_source")(**source_options)
        logpath.logpath[0][0] = new_source
        syslog_ng.reload(config)

        if transport == "udp":
            # The datagram protocol is unchanged; refreshed parse options must apply.
            send_message(old_client, new_driver, "after-reload")
            assert destination.read_log() == "after-reload\n"
        else:
            # Existing connections cannot change their framing protocol; discard them on reload.
            old_client.settimeout(5)
            assert old_client.recv(1) == b""

            new_client = open_client()
            try:
                send_message(new_client, new_driver, "after-reload")
                assert destination.read_log() == "after-reload\n"
            finally:
                new_client.close()
    finally:
        old_client.close()
