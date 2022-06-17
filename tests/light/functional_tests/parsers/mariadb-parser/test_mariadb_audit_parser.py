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


test_parameters = [
    (
        "<190>Apr 13 14:43:13 mysql-server_auditing: columnstore-1 eff8a68bcd7f,user1,172.18.0.1,32,394,QUERY,syslog_ng,'SELECT * FROM test WHERE 0=1',1146",
        {
            "PRI": "190",
            "DATE": "Apr 13 14:43:13",
            "_mariadb": {
                "username": "user1",
                "syslog_info": "columnstore-1",
                "serverhost": "eff8a68bcd7f",
                "retcode": "1146",
                "queryid": "394",
                "operation": "QUERY",
                "object": "SELECT * FROM test WHERE 0=1",
                "host": "172.18.0.1",
                "database": "syslog_ng",
                "connectionid": "32",
            },
            "MSG": r"columnstore-1 eff8a68bcd7f,user1,172.18.0.1,32,394,QUERY,syslog_ng,'SELECT * FROM test WHERE 0=1',1146",
        },
    ),
    (
        "<190>Sep 14 17:46:51 centos mysql-server_auditing: columnstore-1 centos,root,localhost,11,117,QUERY,loans,'SELECT grade, AVG(loan_amnt) avg,FROM loanstats GROUP BY grade ORDER BY grade',0",
        {
            "PRI": "190",
            "DATE": "Sep 14 17:46:51",
            "_mariadb": {
                "username": "root",
                "syslog_info": "columnstore-1",
                "serverhost": "centos",
                "retcode": "0",
                "queryid": "117",
                "operation": "QUERY",
                "object": "SELECT grade, AVG(loan_amnt) avg,FROM loanstats GROUP BY grade ORDER BY grade",
                "host": "localhost",
                "database": "loans",
                "connectionid": "11",
            },
            "MSG": r"columnstore-1 centos,root,localhost,11,117,QUERY,loans,'SELECT grade, AVG(loan_amnt) avg,FROM loanstats GROUP BY grade ORDER BY grade',0",
        },
    ),
    (
        "<190>Apr 29 13:56:36 mysql-server_auditing: docker eff8a68bcd7f,root,172.18.0.3,13,0,CONNECT,,,0",
        {
            "PRI": "190",
            "DATE": "Apr 29 13:56:36",
            "_mariadb": {
                "username": "root",
                "syslog_info": "docker",
                "serverhost": "eff8a68bcd7f",
                "retcode": "0",
                "queryid": "0",
                "operation": "CONNECT",
                "object": "",
                "host": "172.18.0.3",
                "database": "",
                "connectionid": "13",
            },
            "MSG": r"docker eff8a68bcd7f,root,172.18.0.3,13,0,CONNECT,,,0",
        },
    ),
    (
        "<190>Apr 29 13:56:16 mysql-server_auditing: docker eff8a68bcd7f,user1,172.18.0.3,11,58,QUERY,syslog_ng,'insert into test (date, host, program, message) VALUES(Apr 29 15:55:24, locohost, test-program, foo fighters bar baz)',0",
        {
            "PRI": "190",
            "DATE": "Apr 29 13:56:16",
            "_mariadb": {
                "username": "user1",
                "syslog_info": "docker",
                "serverhost": "eff8a68bcd7f",
                "retcode": "0",
                "queryid": "58",
                "operation": "QUERY",
                "object": "insert into test (date, host, program, message) VALUES(Apr 29 15:55:24, locohost, test-program, foo fighters bar baz)",
                "host": "172.18.0.3",
                "database": "syslog_ng",
                "connectionid": "11",
            },
            "MSG": r"docker eff8a68bcd7f,user1,172.18.0.3,11,58,QUERY,syslog_ng,'insert into test (date, host, program, message) VALUES(Apr 29 15:55:24, locohost, test-program, foo fighters bar baz)',0",
        },
    ),
]


@pytest.mark.parametrize(
    "input_message, expected_kv_pairs",
    test_parameters,
    ids=range(len(test_parameters)),
)
def test_mariadb_audit_parser(config, port_allocator, syslog_ng, input_message, expected_kv_pairs):
    config.add_include("scl.conf")

    network_source = config.create_network_source(ip="localhost", port=port_allocator())
    mariadb_audit_parser = config.create_mariadb_audit_parser()
    file_destination = config.create_file_destination(
        file_name="output.log",
        template=config.stringify("$(format-json --scope everything)" + "\n"),
    )
    config.create_logpath(
        statements=[network_source, mariadb_audit_parser, file_destination],
    )

    syslog_ng.start(config)
    network_source.write_log(input_message)

    output_json = json.loads(file_destination.read_log())

    assert output_json["PRI"] == expected_kv_pairs["PRI"]
    assert output_json["DATE"] == expected_kv_pairs["DATE"]

    assert output_json["_mariadb"]["syslog_info"] == expected_kv_pairs["_mariadb"]["syslog_info"]
    assert output_json["_mariadb"]["serverhost"] == expected_kv_pairs["_mariadb"]["serverhost"]
    assert output_json["_mariadb"]["retcode"] == expected_kv_pairs["_mariadb"]["retcode"]
    assert output_json["_mariadb"]["queryid"] == expected_kv_pairs["_mariadb"]["queryid"]
    assert output_json["_mariadb"]["operation"] == expected_kv_pairs["_mariadb"]["operation"]
    assert output_json["_mariadb"]["object"] == expected_kv_pairs["_mariadb"]["object"]
    assert output_json["_mariadb"]["host"] == expected_kv_pairs["_mariadb"]["host"]
    assert output_json["_mariadb"]["database"] == expected_kv_pairs["_mariadb"]["database"]
    assert output_json["_mariadb"]["connectionid"] == expected_kv_pairs["_mariadb"]["connectionid"]

    assert output_json["MSG"] == expected_kv_pairs["MSG"]
