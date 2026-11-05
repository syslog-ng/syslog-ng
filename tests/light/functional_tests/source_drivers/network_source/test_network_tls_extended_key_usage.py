#!/usr/bin/env python
#############################################################################
# Copyright (c) 2026 Akos Zalavary <akos.zalavary@logiqum.com>
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

# Self-signed client certificates in tests/light/shared_files, differing only in their extended key usage.
CLIENT_CERT_SERVER_AUTH = "client-eku-serverauth"
CLIENT_CERT_CLIENT_AUTH = "client-eku-clientauth"
CLIENT_CERT_BOTH = "client-eku-bothauth"
CLIENT_CERT_NO_EKU = "client-eku-noeku"

TLS_VERSIONS = [
    pytest.param(ssl.TLSVersion.TLSv1_2, id="tls1.2"),
    pytest.param(ssl.TLSVersion.TLSv1_3, id="tls1.3"),
]


class _Rejected(Exception):
    pass


def _send_message(testcase_parameters, port, tls_version, client_cert, text):
    """Connect with the given client certificate and send one line.

    Raises _Rejected if the server refused the connection. With TLS 1.3 the client finishes its handshake
    before the server has verified the client certificate, so the alert arrives on the first read after the
    write; the helper therefore also waits for the server to either close the connection or stay silent.
    """
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    context.check_hostname = False
    context.verify_mode = ssl.CERT_NONE
    context.minimum_version = tls_version
    context.maximum_version = tls_version
    context.load_cert_chain(
        copy_shared_file(testcase_parameters, f"{client_cert}.crt"),
        copy_shared_file(testcase_parameters, f"{client_cert}.key"),
    )

    raw_socket = socket.create_connection(("127.0.0.1", port))
    try:
        try:
            client = context.wrap_socket(raw_socket, server_hostname="localhost")
        except ssl.SSLError as error:
            raise _Rejected(f"handshake: {error}") from error
        try:
            client.settimeout(2)
            client.sendall(f"{text}\n".encode("ascii"))
            try:
                if client.recv(1) == b"":
                    raise _Rejected("connection closed by the server")
            except socket.timeout:
                pass  # the server never answers on an accepted connection
        except (ssl.SSLError, ConnectionError) as error:
            raise _Rejected(f"after handshake: {error}") from error
        finally:
            client.close()
    finally:
        raw_socket.close()


def _start_source(config, syslog_ng, port_allocator, testcase_parameters, extended_key_usage_verify):
    port = port_allocator()
    source = config.create_network_source(
        ip="localhost",
        port=port,
        transport='"tls"',
        flags="no-parse",
        tls={
            "key-file": copy_shared_file(testcase_parameters, "server.key"),
            "cert-file": copy_shared_file(testcase_parameters, "server.crt"),
            # optional-untrusted maps to SSL_VERIFY_NONE: the server would not even ask for a client certificate.
            "peer-verify": '"required-untrusted"',
            "extended-key-usage-verify": extended_key_usage_verify,
        },
    )
    destination = config.create_file_destination(file_name="output.log", template=r'"${MESSAGE}\n"')
    config.create_logpath(statements=[source, destination])
    syslog_ng.start(config)
    return port, destination


@pytest.mark.parametrize("tls_version", TLS_VERSIONS)
def test_source_rejects_client_certificate_without_client_auth_eku(
    config, syslog_ng, port_allocator, testcase_parameters, tls_version,
):
    port, destination = _start_source(config, syslog_ng, port_allocator, testcase_parameters, "yes")

    with pytest.raises(_Rejected) as rejection:
        _send_message(testcase_parameters, port, tls_version, CLIENT_CERT_SERVER_AUTH, "from-serverauth-only-cert")
    if tls_version == ssl.TLSVersion.TLSv1_2:
        # TLS 1.2 verifies the client certificate before the handshake completes on the client side.
        assert str(rejection.value).startswith("handshake:")

    # The rejected connection delivered nothing: the first message that arrives is the control message.
    _send_message(testcase_parameters, port, tls_version, CLIENT_CERT_CLIENT_AUTH, "from-clientauth-cert")
    assert destination.read_log() == "from-clientauth-cert\n"


@pytest.mark.parametrize("tls_version", TLS_VERSIONS)
@pytest.mark.parametrize(
    "client_cert",
    [
        pytest.param(CLIENT_CERT_CLIENT_AUTH, id="clientAuth-only"),
        pytest.param(CLIENT_CERT_BOTH, id="serverAuth+clientAuth"),
        pytest.param(CLIENT_CERT_NO_EKU, id="no-eku-extension"),
    ],
)
def test_source_accepts_client_certificate_allowed_by_extended_key_usage_verify(
    config, syslog_ng, port_allocator, testcase_parameters, tls_version, client_cert,
):
    port, destination = _start_source(config, syslog_ng, port_allocator, testcase_parameters, "yes")

    _send_message(testcase_parameters, port, tls_version, client_cert, "accepted-message")

    assert destination.read_log() == "accepted-message\n"


@pytest.mark.parametrize("tls_version", TLS_VERSIONS)
def test_source_accepts_any_client_certificate_when_extended_key_usage_verify_is_off(
    config, syslog_ng, port_allocator, testcase_parameters, tls_version,
):
    port, destination = _start_source(config, syslog_ng, port_allocator, testcase_parameters, "no")

    _send_message(testcase_parameters, port, tls_version, CLIENT_CERT_SERVER_AUTH, "from-serverauth-only-cert")

    assert destination.read_log() == "from-serverauth-only-cert\n"
