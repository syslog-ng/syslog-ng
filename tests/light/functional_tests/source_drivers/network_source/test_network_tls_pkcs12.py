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
import pytest

from src.common.blocking import wait_until_true
from src.common.file import copy_shared_file


@pytest.mark.parametrize(
    ("pkcs12_file", "password"),
    [
        ("server.p12", None),
        ("server-protected-asdfg.p12", "asdfg"),
    ],
    ids=["no-password", "password-from-credential-store"],
)
def test_network_tls_pkcs12(config, syslog_ng, syslog_ng_ctl, port_allocator, loggen, testcase_parameters, pkcs12_file, password):
    pkcs12_path = copy_shared_file(testcase_parameters, pkcs12_file)
    network_source = config.create_network_source(
        ip="localhost",
        port=port_allocator(),
        transport='"tls"',
        flags="no-parse",
        tls={
            "pkcs12-file": pkcs12_path,
            "peer-verify": '"optional-untrusted"',
        },
    )
    file_destination = config.create_file_destination(file_name="output.log", template=r'"${MESSAGE}\n"')
    config.create_logpath(statements=[network_source, file_destination])

    syslog_ng.start(config)
    if password is not None:
        # the password of a pkcs12-file() is a credential named after the file, like for key-file()
        syslog_ng_ctl.credentials_add(credential=pkcs12_path, secret=password)

    loggen.start(network_source.options["ip"], network_source.options["port"], number=1, use_ssl=True, dont_parse=True)
    wait_until_true(lambda: loggen.get_sent_message_count() == 1)

    assert "seq: 0000000000" in file_destination.read_log()
