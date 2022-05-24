#!/usr/bin/env python
#############################################################################
# Copyright (c) 2022 One Identity
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
import json

import pytest

from src.common.network_operations import get_hostname


test_parameters = [
    (
        r"<189>29: foo: *Apr 29 13:58:40.411: %SYS-5-CONFIG_I: Configured from console by console",
        {
            "PRI": "189",
            "HOST": "foo",
            "DATE": "Apr 29 13:58:40",
            "MSEC": "411",
            "_cisco": {
                "facility": "SYS",
                "severity": "5",
                "mnemonic": "CONFIG_I",
            },
            "MSG": r"%SYS-5-CONFIG_I: Configured from console by console",
        },
    ),
    (
        r"<190>30: foo: *Apr 29 13:58:46.411: %SYS-6-LOGGINGHOST_STARTSTOP: Logging to host 192.168.1.239 stopped - CLI initiated",
        {
            "PRI": "190",
            "HOST": "foo",
            "DATE": "Apr 29 13:58:46",
            "MSEC": "411",
            "_cisco": {
                "facility": "SYS",
                "severity": "6",
                "mnemonic": "LOGGINGHOST_STARTSTOP",
            },
            "MSG": r"%SYS-6-LOGGINGHOST_STARTSTOP: Logging to host 192.168.1.239 stopped - CLI initiated",
        },
    ),
    (
        r"<190>31: foo: *Apr 29 13:58:46.411: %SYS-6-LOGGINGHOST_STARTSTOP: Logging to host 192.168.1.239 started - CLI initiated",
        {
            "PRI": "190",
            "HOST": "foo",
            "DATE": "Apr 29 13:58:46",
            "MSEC": "411",
            "_cisco": {
                "facility": "SYS",
                "severity": "6",
                "mnemonic": "LOGGINGHOST_STARTSTOP",
            },
            "MSG": r"%SYS-6-LOGGINGHOST_STARTSTOP: Logging to host 192.168.1.239 started - CLI initiated",
        },
    ),
    (
        r"<189>35: *Apr 29 14:00:16.059: %SYS-5-CONFIG_I: Configured from console by console",
        {
            "PRI": "189",
            "HOST": get_hostname(),
            "DATE": "Apr 29 14:00:16",
            "MSEC": "059",
            "_cisco": {
                "facility": "SYS",
                "severity": "5",
                "mnemonic": "CONFIG_I",
            },
            "MSG": r"%SYS-5-CONFIG_I: Configured from console by console",
        },
    ),
    (
        r"<190>32: foo: *Apr 29 13:58:46.411: %SYSMGR-STANDBY-3-SHUTDOWN_START: The System Manager has started the shutdown procedure.",
        {
            "PRI": "190",
            "HOST": "foo",
            "DATE": "Apr 29 13:58:46",
            "MSEC": "411",
            "_cisco": {
                "facility": "SYSMGR-STANDBY",
                "severity": "3",
                "mnemonic": "SHUTDOWN_START",
            },
            "MSG": r"%SYSMGR-STANDBY-3-SHUTDOWN_START: The System Manager has started the shutdown procedure.",
        },
    ),
    (
        r"<180>782431: machine1: .Nov 18 21:03:22.631 GMT: %CDP-4-NATIVE_VLAN_MISMATCH: Native VLAN mismatch discovered on TenGigabitEthernet.",
        {
            "PRI": "180",
            "HOST": "machine1",
            "DATE": "Nov 18 21:03:22",
            "MSEC": "631",
            "_cisco": {
                "facility": "CDP",
                "severity": "4",
                "mnemonic": "NATIVE_VLAN_MISMATCH",
            },
            "MSG": r"%CDP-4-NATIVE_VLAN_MISMATCH: Native VLAN mismatch discovered on TenGigabitEthernet.",
        },
    ),
    (
        r"<166>2022-02-16T15:31:53Z na-zy-int-fp1140-p02 : %FTD-6-305012: Teardown dynamic TCP translation from FOO-WAN_IN:10.44.60.80/59877 to FOO-OUTSIDE:6.7.8.9/59877 duration 0:01:01",
        {
            "PRI": "166",
            "HOST": "na-zy-int-fp1140-p02",
            "DATE": "Feb 16 15:31:53",
            "MSEC": "000",
            "_cisco": {
                "facility": "FTD",
                "severity": "6",
                "mnemonic": "305012",
            },
            "MSG": r"%FTD-6-305012: Teardown dynamic TCP translation from FOO-WAN_IN:10.44.60.80/59877 to FOO-OUTSIDE:6.7.8.9/59877 duration 0:01:01",
        },
    ),
]


@pytest.mark.parametrize(
    "input_message, expected_kv_pairs",
    test_parameters,
    ids=range(len(test_parameters)),
)
def test_cisco_parser(config, syslog_ng, input_message, expected_kv_pairs):
    config.add_include("scl.conf")

    generator_source = config.create_example_msg_generator_source(num=1, template=config.stringify(input_message))
    cisco_parser = config.create_cisco_parser()
    file_destination = config.create_file_destination(
        file_name="output.log",
        template=config.stringify("$(format-json --scope everything)" + "\n"),
    )
    config.create_logpath(statements=[generator_source, cisco_parser, file_destination])

    syslog_ng.start(config)

    output_json = json.loads(file_destination.read_log())

    assert output_json["PRI"] == expected_kv_pairs["PRI"]
    assert output_json["HOST"] == expected_kv_pairs["HOST"]
    assert output_json["DATE"] == expected_kv_pairs["DATE"]
    assert output_json["MSEC"] == expected_kv_pairs["MSEC"]
    assert output_json["_cisco"]["facility"] == expected_kv_pairs["_cisco"]["facility"]
    assert output_json["_cisco"]["severity"] == expected_kv_pairs["_cisco"]["severity"]
    assert output_json["_cisco"]["mnemonic"] == expected_kv_pairs["_cisco"]["mnemonic"]
    assert output_json["MSG"] == expected_kv_pairs["MSG"]
