#!/usr/bin/env python
#############################################################################
# Copyright (c) 2023 Szilard Parrag
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
        "2023-09-20 16:50:02.165 CET,,,407764,,650b069a.638d4,2,,2023-09-20 16:50:02 CET,,0,DEBUG3,00000,\"starting PostgreSQL 15.4 (Ubuntu 15.4-0ubuntu0.23.04.1) on x86_64-pc-linux-gnu, compiled by gcc (Ubuntu 12.3.0-1ubuntu1~23.04) 12.3.0, 64-bit\",,,,,,,,,\"\",\"postmaster\",,0",
        {
            "DATE": "Sep 20 16:50:02",
            "HOST_FROM": "localhost",
            "MSG": "starting PostgreSQL 15.4 (Ubuntu 15.4-0ubuntu0.23.04.1) on x86_64-pc-linux-gnu, compiled by gcc (Ubuntu 12.3.0-1ubuntu1~23.04) 12.3.0, 64-bit",
            "PID": 407764,
            "SEVERITY": "debug",
            "_pgsql": {
                "application_name": "",
                "command_tag": "",
                "connection_from": "localhost",
                "context": "",
                "database": "",
                "detail": "",
                "hint": "",
                "internal_query": "",
                "location": "",
                "message": "starting PostgreSQL 15.4 (Ubuntu 15.4-0ubuntu0.23.04.1) on x86_64-pc-linux-gnu, compiled by gcc (Ubuntu 12.3.0-1ubuntu1~23.04) 12.3.0, 64-bit",
                "pid": 407764,
                "query": "",
                "session_id": "650b069a.638d4",
                "session_line_num": 2,
                "session_start_time": "2023-09-20 16:50:02 CET",
                "severity": "LOG",
                "sql_state_code": "00000",
                "timestamp": "2023-09-20 16:50:02.165 CET",
                "transaction_id": "0",
                "username": "",
                "virtual_transaction_id": "",
            },
        },
    ),
    (
        r'2023-08-08 12:05:52.805 UTC,,,22113,,64d22fa0.5661,1,,2023-08-08 12:05:52 UTC,23/74060,0,LOG,00000,"automatic vacuum of table ""tablename"": index scans: 0 pages: 0 removed, 4 remain, 0 skipped due to pins, 0 skipped frozen tuples: 114 removed, 268 remain, 0 are dead but not yet removable, oldest xmin: 149738000 buffer usage: 97 hits, 0 misses, 6 dirtied avg read rate: 0.000 MB/s, avg write rate: 114.609 MB/s system usage: CPU: user: 0.00 s, system: 0.00 s, elapsed: 0.00 s",,,,,,,,,""',
        {
            "DATE": "Aug  8 12:05:52",
            "HOST_FROM": "localhost",
            "MSG": r'automatic vacuum of table "tablename": index scans: 0 pages: 0 removed, 4 remain, 0 skipped due to pins, 0 skipped frozen tuples: 114 removed, 268 remain, 0 are dead but not yet removable, oldest xmin: 149738000 buffer usage: 97 hits, 0 misses, 6 dirtied avg read rate: 0.000 MB/s, avg write rate: 114.609 MB/s system usage: CPU: user: 0.00 s, system: 0.00 s, elapsed: 0.00 s',
            "PID": 22113,
            "SEVERITY": "info",
            "_pgsql": {
                "application_name": "",
                "command_tag": "",
                "connection_from": "localhost",
                "context": "",
                "database": "",
                "detail": "",
                "hint": "",
                "internal_query": "",
                "location": "",
                "message": r'automatic vacuum of table "tablename": index scans: 0 pages: 0 removed, 4 remain, 0 skipped due to pins, 0 skipped frozen tuples: 114 removed, 268 remain, 0 are dead but not yet removable, oldest xmin: 149738000 buffer usage: 97 hits, 0 misses, 6 dirtied avg read rate: 0.000 MB/s, avg write rate: 114.609 MB/s system usage: CPU: user: 0.00 s, system: 0.00 s, elapsed: 0.00 s',
                "pid": 22113,
                "query": "",
                "session_id": "64d22fa0.5661",
                "session_line_num": 1,
                "session_start_time": "2023-08-08 12:05:52 UTC",
                "severity": "LOG",
                "sql_state_code": "00000",
                "timestamp": "2023-08-08 12:05:52.805 UTC",
                "transaction_id": "0",
                "username": "",
                "virtual_transaction_id": "",
            },
        },
    ),
    (
        r'2023-08-08 12:05:52.805 UTC,,,22113,,64d22fa0.5661,1,,2023-08-08 12:05:52 UTC,23/74060,0,WARNING,00000,"automatic vacuum of table ""tablename"": index scans: 0 pages: 0 removed, 4 remain, 0 skipped due to pins, 0 skipped frozen tuples: 114 removed, 268 remain, 0 are dead but not yet removable, oldest xmin: 149738000 buffer usage: 97 hits, 0 misses, 6 dirtied avg read rate: 0.000 MB/s, avg write rate: 114.609 MB/s system usage: CPU: user: 0.00 s, system: 0.00 s, elapsed: 0.00 s",,,,,,,,,""',
        {
            "DATE": "Aug  8 12:05:52",
            "HOST_FROM": "localhost",
            "MSG": r'automatic vacuum of table "tablename": index scans: 0 pages: 0 removed, 4 remain, 0 skipped due to pins, 0 skipped frozen tuples: 114 removed, 268 remain, 0 are dead but not yet removable, oldest xmin: 149738000 buffer usage: 97 hits, 0 misses, 6 dirtied avg read rate: 0.000 MB/s, avg write rate: 114.609 MB/s system usage: CPU: user: 0.00 s, system: 0.00 s, elapsed: 0.00 s',
            "PID": 22113,
            "SEVERITY": "notice",
            "_pgsql": {
                "application_name": "",
                "command_tag": "",
                "connection_from": "localhost",
                "context": "",
                "database": "",
                "detail": "",
                "hint": "",
                "internal_query": "",
                "location": "",
                "message": r'automatic vacuum of table "tablename": index scans: 0 pages: 0 removed, 4 remain, 0 skipped due to pins, 0 skipped frozen tuples: 114 removed, 268 remain, 0 are dead but not yet removable, oldest xmin: 149738000 buffer usage: 97 hits, 0 misses, 6 dirtied avg read rate: 0.000 MB/s, avg write rate: 114.609 MB/s system usage: CPU: user: 0.00 s, system: 0.00 s, elapsed: 0.00 s',
                "pid": 22113,
                "query": "",
                "session_id": "64d22fa0.5661",
                "session_line_num": 1,
                "session_start_time": "2023-08-08 12:05:52 UTC",
                "severity": "WARNING",
                "sql_state_code": "00000",
                "timestamp": "2023-08-08 12:05:52.805 UTC",
                "transaction_id": "0",
                "username": "",
                "virtual_transaction_id": "",
            },
        },
    ),
    (
        r'2023-08-08 12:05:52.805 UTC,,,22113,,64d22fa0.5661,1,,2023-08-08 12:05:52 UTC,23/74060,0,PANIC,00000,"automatic vacuum of table ""tablename"": index scans: 0 pages: 0 removed, 4 remain, 0 skipped due to pins, 0 skipped frozen tuples: 114 removed, 268 remain, 0 are dead but not yet removable, oldest xmin: 149738000 buffer usage: 97 hits, 0 misses, 6 dirtied avg read rate: 0.000 MB/s, avg write rate: 114.609 MB/s system usage: CPU: user: 0.00 s, system: 0.00 s, elapsed: 0.00 s",,,,,,,,,""',
        {
            "DATE": "Aug  8 12:05:52",
            "HOST_FROM": "localhost",
            "MSG": r'automatic vacuum of table "tablename": index scans: 0 pages: 0 removed, 4 remain, 0 skipped due to pins, 0 skipped frozen tuples: 114 removed, 268 remain, 0 are dead but not yet removable, oldest xmin: 149738000 buffer usage: 97 hits, 0 misses, 6 dirtied avg read rate: 0.000 MB/s, avg write rate: 114.609 MB/s system usage: CPU: user: 0.00 s, system: 0.00 s, elapsed: 0.00 s',
            "PID": 22113,
            "SEVERITY": "crit",
            "_pgsql": {
                "application_name": "",
                "command_tag": "",
                "connection_from": "localhost",
                "context": "",
                "database": "",
                "detail": "",
                "hint": "",
                "internal_query": "",
                "location": "",
                "message": r'automatic vacuum of table "tablename": index scans: 0 pages: 0 removed, 4 remain, 0 skipped due to pins, 0 skipped frozen tuples: 114 removed, 268 remain, 0 are dead but not yet removable, oldest xmin: 149738000 buffer usage: 97 hits, 0 misses, 6 dirtied avg read rate: 0.000 MB/s, avg write rate: 114.609 MB/s system usage: CPU: user: 0.00 s, system: 0.00 s, elapsed: 0.00 s',
                "pid": 22113,
                "query": "",
                "session_id": "64d22fa0.5661",
                "session_line_num": 1,
                "session_start_time": "2023-08-08 12:05:52 UTC",
                "severity": "WARNING",
                "sql_state_code": "00000",
                "timestamp": "2023-08-08 12:05:52.805 UTC",
                "transaction_id": "0",
                "username": "",
                "virtual_transaction_id": "",
            },
        },
    ),
    (
        # Less fields than specified in the scl
        r'2023-08-08 12:05:52.805 UTC,,,22113,,64d22fa0.5661,1,,2023-08-08 12:05:52 UTC,23/74060,0,LOG,00000,',
        {

            "DATE": "Aug  8 12:05:52",
            "HOST_FROM": "localhost",
            "PID": 22113,
            "SEVERITY": "info",
            "_pgsql": {
                "application_name": "",
                "command_tag": "",
                "connection_from": "localhost",
                "context": "",
                "database": "",
                "detail": "",
                "hint": "",
                "internal_query": "",
                "location": "",
                "pid": 22113,
                "query": "",
                "session_id": "64d22fa0.5661",
                "session_line_num": 1,
                "session_start_time": "2023-08-08 12:05:52 UTC",
                "severity": "LOG",
                "sql_state_code": "00000",
            },
        },
    ),
    (
        # Log with query_id(v14+), backend_type(v13+)
        r'2023-11-03 16:32:35.084 CET,"postgres","bench_test",3451998,"[local]",65451258.34ac5e,632376,"UPDATE",2023-11-03 16:31:36 CET,6/45171,228887,LOG,00000,"duration: 0.081 ms",,,,,,,,,"pgbench","client backend",,1521477082073268809',
        {
            "DATE": "Nov  3 16:32:35",
            "HOST_FROM": "localhost",
            "MSG": r'automatic vacuum of table "tablename": index scans: 0 pages: 0 removed, 4 remain, 0 skipped due to pins, 0 skipped frozen tuples: 114 removed, 268 remain, 0 are dead but not yet removable, oldest xmin: 149738000 buffer usage: 97 hits, 0 misses, 6 dirtied avg read rate: 0.000 MB/s, avg write rate: 114.609 MB/s system usage: CPU: user: 0.00 s, system: 0.00 s, elapsed: 0.00 s',
            "PID": 3451998,
            "SEVERITY": "info",
            "_pgsql": {
                "application_name": "",
                "command_tag": "",
                "connection_from": "localhost",
                "context": "",
                "database": "",
                "detail": "",
                "hint": "",
                "internal_query": "",
                "location": "",
                "message": r'automatic vacuum of table "tablename": index scans: 0 pages: 0 removed, 4 remain, 0 skipped due to pins, 0 skipped frozen tuples: 114 removed, 268 remain, 0 are dead but not yet removable, oldest xmin: 149738000 buffer usage: 97 hits, 0 misses, 6 dirtied avg read rate: 0.000 MB/s, avg write rate: 114.609 MB/s system usage: CPU: user: 0.00 s, system: 0.00 s, elapsed: 0.00 s',
                "pid": 22113,
                "query": "",
                "session_id": "65451258.34ac5e",
                "session_line_num": 632376,
                "session_start_time": "2023-11-03 16:31:36 CET",
                "severity": "LOG",
                "sql_state_code": "00000",
                "timestamp": "2023-11-03 16:32:35.084 CET",
                "transaction_id": "0",
                "username": "",
                "virtual_transaction_id": "",
                "query_id": 1521477082073268809,
                "backend_type": "client backend",
            },
        },
    ),
]


@pytest.mark.parametrize(
    "input_message, expected_kv_pairs",
    test_parameters,
    ids=range(len(test_parameters)),
)
def test_postgresql_csvlog_parser(config, port_allocator, syslog_ng, input_message, expected_kv_pairs):
    config.add_include("scl.conf")

    network_source = config.create_network_source(ip="localhost", port=port_allocator(), flags="no-parse")
    pgsql_csvlog_parser = config.create_postgresql_csvlog_parser()
    file_destination = config.create_file_destination(
        file_name="output.log",
        template=config.stringify("$(format-json --scope everything)" + "\n"),
    )
    config.create_logpath(
        statements=[network_source, pgsql_csvlog_parser, file_destination],
    )

    syslog_ng.start(config)
    network_source.write_log(input_message)

    output_json = json.loads(file_destination.read_log())

    assert output_json["DATE"] == expected_kv_pairs["DATE"]
    assert output_json["HOST_FROM"] == expected_kv_pairs["HOST_FROM"]
    assert output_json["PID"] == expected_kv_pairs["PID"]
    assert output_json["SEVERITY"] == expected_kv_pairs["SEVERITY"]

    assert output_json["_pgsql"]["session_id"] == expected_kv_pairs["_pgsql"]["session_id"]
    assert output_json["_pgsql"]["session_line_num"] == expected_kv_pairs["_pgsql"]["session_line_num"]
    assert output_json["_pgsql"]["session_start_time"] == expected_kv_pairs["_pgsql"]["session_start_time"]

    # If field is filled
    if "timestamp" in expected_kv_pairs["_pgsql"]:
        assert output_json["_pgsql"]["timestamp"] == expected_kv_pairs["_pgsql"]["timestamp"]

    # Optional, extra fields
    extra_fields = ["query_id", "backend_type"]
    for extra_field in extra_fields:
        if extra_field in expected_kv_pairs["_pgsql"]:
            assert output_json["_pgsql"][extra_field] == expected_kv_pairs["_pgsql"][extra_field]
