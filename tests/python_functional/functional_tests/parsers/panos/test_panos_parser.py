#!/usr/bin/env python
#############################################################################
# Copyright (c) 2020 Balabit
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

test_parameters_raw = [
    # Testing SYSTEM branch
    (r"""<12>Apr 14 16:48:54 paloalto.test.net 1,2020/04/14 16:48:54,unknown,SYSTEM,auth,0,2020/04/14 16:48:54,,auth-fail,,0,0,general,medium,failed authentication for user 'admin'. Reason: Invalid username/password. From: 10.0.10.55.,1718,0x0,0,0,0,0,,paloalto""", "<${PROGRAM}><${.panos.type}><${.panos.eventid}><${.panos.device_name}>", "<paloalto_panos><SYSTEM><auth-fail><paloalto>"),
    # Testing SYSTEM branch with extra additions at the end. Should be accepted.
    (r"""<12>Apr 14 16:48:54 paloalto.test.net 1,2020/04/14 16:48:54,unknown,SYSTEM,auth,0,2020/04/14 16:48:54,,auth-fail,,0,0,general,medium,failed authentication for user 'admin'. Reason: Invalid username/password. From: 10.0.10.55.,1718,0x0,0,0,0,0,,paloalto,foo,bar""", "<${PROGRAM}><${.panos.type}><${.panos.eventid}><${.panos.device_name}>", "<paloalto_panos><SYSTEM><auth-fail><paloalto>"),
    # Testing CONFIG branch
    (r"""<14>Apr 14 16:54:18 paloalto.test.net 1,2020/04/14 16:54:18,unknown,CONFIG,0,0,2020/04/14 16:54:18,10.0.10.55,,set,admin,Web,Succeeded,deviceconfig system,127,0x0,0,0,0,0,,paloalto""", "<${PROGRAM}><${.panos.type}><${.panos.path}><${.panos.device_name}>", "<paloalto_panos><CONFIG><deviceconfig system><paloalto>"),
    # Testing CONFIG branch, custom logs
    (r"""<14>Apr 14 16:54:18 paloalto.test.net 1,2020/04/14 16:54:18,unknown,CONFIG,0,0,2020/04/14 16:54:18,10.0.10.55,,set,admin,Web,Succeeded,deviceconfig system,before,after,127,0x0,0,0,0,0,,paloalto""", "<${PROGRAM}><${.panos.type}><${.panos.path}><${.panos.device_name}>", "<paloalto_panos><CONFIG><deviceconfig system><paloalto>"),
]


@pytest.mark.parametrize(
    "input_message, template, expected_value", test_parameters_raw,
    ids=list(map(str, range(len(test_parameters_raw)))),
)
def test_panos_parser(config, syslog_ng, input_message, template, expected_value):
    config.add_include("scl.conf")

    generator_source = config.create_example_msg_generator_source(num=1, template=config.stringify(input_message))
    checkpoint_parser = config.create_panos_parser()

    file_destination = config.create_file_destination(file_name="output.log", template=config.stringify(template + "\n"))
    config.create_logpath(statements=[generator_source, checkpoint_parser, file_destination])

    syslog_ng.start(config)
    assert file_destination.read_log().strip() == expected_value
