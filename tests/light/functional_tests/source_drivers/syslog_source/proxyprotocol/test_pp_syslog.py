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
from pathlib import Path

from src.common.blocking import wait_until_true
from src.common.file import copy_shared_file
from src.common.file import File
from src.common.random_id import get_unique_id


def _test_pp(config, syslog_ng, syslog_ng_ctl, port_allocator, loggen, testcase_parameters, transport, input_messages, number_of_messages, expected_messages, template=None, password=None):
    if password:
        server_key_path = copy_shared_file(testcase_parameters, "server-protected-asdfg.key")
        server_cert_path = copy_shared_file(testcase_parameters, "server-protected-asdfg.crt")
    else:
        server_key_path = copy_shared_file(testcase_parameters, "server.key")
        server_cert_path = copy_shared_file(testcase_parameters, "server.crt")
    output_file = "output.log"
    use_ssl = True if "tls" in transport else None
    use_inet = None if use_ssl else True
    use_passthrough = True if "passthrough" in transport else None

    if (use_ssl):
        syslog_source = config.create_syslog_source(
            ip="localhost",
            port=port_allocator(),
            transport=transport,
            flags="no-parse",
            tls={
                "key-file": server_key_path,
                "cert-file": server_cert_path,
                "peer-verify": '"optional-untrusted"',
            },
        )
    else:
        syslog_source = config.create_syslog_source(
            ip="localhost",
            port=port_allocator(),
            transport=transport,
            flags="no-parse",
        )
    if template:
        file_destination = config.create_file_destination(file_name=output_file, template=template)
    else:
        file_destination = config.create_file_destination(file_name=output_file)
    config.create_logpath(statements=[syslog_source, file_destination])

    syslog_ng.start(config)
    if password is not None:
        syslog_ng_ctl.credentials_add(credential=server_key_path, secret=password)

    loggen_input_file_path = Path("loggen_input_{}.txt".format(get_unique_id()))
    loggen_input_file = File(loggen_input_file_path)
    loggen_input_file.write_content_and_close(input_messages)
    loggen.start(
        syslog_source.options["ip"], syslog_source.options["port"],
        number=number_of_messages,
        dont_parse=True,
        read_file=str(loggen_input_file_path),
        syslog_proto=True,
        inet=use_inet,
        use_ssl=use_ssl,
        proxied=1,
        proxied_tls_passthrough=use_passthrough,
        proxy_src_ip="1.1.1.1", proxy_dst_ip="2.2.2.2", proxy_src_port="3333", proxy_dst_port="4444",
    )

    # seems proxy header is counted in get_sent_message_count(), so we need '-1'
    wait_until_true(lambda: loggen.get_sent_message_count() == number_of_messages - 1)

    assert file_destination.read_log() == expected_messages


def test_pp_syslog_tcp(config, syslog_ng, syslog_ng_ctl, port_allocator, loggen, testcase_parameters):
    TEMPLATE = r'"${SOURCEIP} ${SOURCEPORT} ${DESTIP} ${DESTPORT} ${MESSAGE}\n"'
    INPUT_MESSAGES = "53 <2>Oct 11 22:14:15 myhostname sshd[1234]: message 0\r\n"
    EXPECTED_MESSAGES = "1.1.1.1 3333 2.2.2.2 4444 <2>Oct 11 22:14:15 myhostname sshd[1234]: message 0\n"
    NUMBER_OF_MESSAGES = 2
    _test_pp(config, syslog_ng, syslog_ng_ctl, port_allocator, loggen, testcase_parameters, '"proxied-tcp"', INPUT_MESSAGES, NUMBER_OF_MESSAGES, EXPECTED_MESSAGES, TEMPLATE)


def test_pp_syslog_tls(config, syslog_ng, syslog_ng_ctl, port_allocator, loggen, testcase_parameters):
    TEMPLATE = r'"${SOURCEIP} ${SOURCEPORT} ${DESTIP} ${DESTPORT} ${MESSAGE}\n"'
    INPUT_MESSAGES = "53 <2>Oct 11 22:14:15 myhostname sshd[1234]: message 0\r\n"
    EXPECTED_MESSAGES = "1.1.1.1 3333 2.2.2.2 4444 <2>Oct 11 22:14:15 myhostname sshd[1234]: message 0\n"
    NUMBER_OF_MESSAGES = 2
    _test_pp(config, syslog_ng, syslog_ng_ctl, port_allocator, loggen, testcase_parameters, '"proxied-tls"', INPUT_MESSAGES, NUMBER_OF_MESSAGES, EXPECTED_MESSAGES, TEMPLATE)


def test_pp_syslog_tls_with_passphrase(config, syslog_ng, syslog_ng_ctl, port_allocator, loggen, testcase_parameters):
    TEMPLATE = r'"${SOURCEIP} ${SOURCEPORT} ${DESTIP} ${DESTPORT} ${MESSAGE}\n"'
    INPUT_MESSAGES = "53 <2>Oct 11 22:14:15 myhostname sshd[1234]: message 0\r\n"
    EXPECTED_MESSAGES = "1.1.1.1 3333 2.2.2.2 4444 <2>Oct 11 22:14:15 myhostname sshd[1234]: message 0\n"
    NUMBER_OF_MESSAGES = 2
    _test_pp(config, syslog_ng, syslog_ng_ctl, port_allocator, loggen, testcase_parameters, '"proxied-tls"', INPUT_MESSAGES, NUMBER_OF_MESSAGES, EXPECTED_MESSAGES, TEMPLATE, password="asdfg")


def test_pp_syslog_tls_passthrough(config, syslog_ng, syslog_ng_ctl, port_allocator, loggen, testcase_parameters):
    TEMPLATE = r'"${SOURCEIP} ${SOURCEPORT} ${DESTIP} ${DESTPORT} ${MESSAGE}\n"'
    INPUT_MESSAGES = "53 <2>Oct 11 22:14:15 myhostname sshd[1234]: message 0\r\n"
    EXPECTED_MESSAGES = "1.1.1.1 3333 2.2.2.2 4444 <2>Oct 11 22:14:15 myhostname sshd[1234]: message 0\n"
    NUMBER_OF_MESSAGES = 2
    _test_pp(config, syslog_ng, syslog_ng_ctl, port_allocator, loggen, testcase_parameters, '"proxied-tls-passthrough"', INPUT_MESSAGES, NUMBER_OF_MESSAGES, EXPECTED_MESSAGES, TEMPLATE)
