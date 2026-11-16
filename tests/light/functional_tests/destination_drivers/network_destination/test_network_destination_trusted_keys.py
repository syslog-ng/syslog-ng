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
import hashlib
import ssl
from pathlib import Path

import pytest

from src.common.blocking import wait_until_true
from src.common.file import copy_shared_file


def _fingerprint(cert_path, algorithm, prefix):
    der = ssl.PEM_cert_to_DER_cert(Path(cert_path).read_text())
    digest = hashlib.new(algorithm, der).hexdigest().upper()
    return prefix + ":".join(digest[i:i + 2] for i in range(0, len(digest), 2))


@pytest.mark.parametrize(
    ("algorithm", "prefix", "matching"),
    [
        ("sha1", "SHA1:", True),
        ("sha256", "SHA256:", True),
        ("sha256", "SHA256:", False),
    ],
    ids=["sha1", "sha256", "sha256-mismatch"],
)
def test_network_destination_trusted_keys(config, syslog_ng, port_allocator, testcase_parameters, algorithm, prefix, matching):
    server_key = copy_shared_file(testcase_parameters, "server.key")
    server_cert = copy_shared_file(testcase_parameters, "server.crt")
    ca_cert = copy_shared_file(testcase_parameters, "ca.crt")

    fingerprint = _fingerprint(server_cert, algorithm, prefix)
    if not matching:
        fingerprint = fingerprint[:-2] + ("01" if fingerprint.endswith("00") else "00")

    port = port_allocator()
    output = Path("output.log").absolute()
    # one instance pins the certificate of its own TLS source; the shared test
    # certificate is expired and issued for CN=server, hence the ssl-options()
    raw_config = f"""@version: {config.get_version()}
source s_tls {{
    network(ip("localhost") port({port}) transport("tls") flags(no-parse)
            tls(key-file("{server_key}") cert-file("{server_cert}") peer-verify("optional-untrusted")));
}};
destination d_file {{ file("{output}"); }};
log {{ source(s_tls); destination(d_file); }};

source s_gen {{ example-msg-generator(num(1) template("pinned")); }};
destination d_tls {{
    network("localhost" port({port}) transport("tls")
            tls(ca-file("{ca_cert}") peer-verify("required-trusted")
                ssl-options("ignore-validity-period", "ignore-hostname-mismatch")
                trusted-keys("{fingerprint}")));
}};
log {{ source(s_gen); destination(d_tls); }};
"""
    config.set_raw_config(raw_config)
    syslog_ng.start(config)

    if matching:
        assert wait_until_true(lambda: output.exists() and "pinned" in output.read_text())
    else:
        syslog_ng.wait_for_message_in_console_log("Certificate valid, but fingerprint constraints were not met, rejecting")
        assert not output.exists() or output.read_text() == ""
