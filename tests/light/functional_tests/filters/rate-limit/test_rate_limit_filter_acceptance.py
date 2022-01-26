#!/usr/bin/env python
#############################################################################
# Copyright (c) 2021 One Identity
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
from src.message_builder.log_message import LogMessage


def generate_messages_with_different_program_fields(bsd_formatter, number_of_all_messages, different_program_fields):
    input_messages = ""
    program_idx = 1
    for message_idx in range(1, number_of_all_messages + 1):
        if program_idx == different_program_fields + 1:
            program_idx = 1
        log_message = LogMessage().program(program_idx).message("message idx: %s" % message_idx)
        input_messages += bsd_formatter.format_message(log_message, add_new_line=True)
        program_idx += 1
    return input_messages


@pytest.mark.parametrize(
    "message_counter, message_rate_by_sec, different_program_fields, rate_limit_rate_by_sec, expected_number_of_matched_messages, expected_number_of_not_matched_messages", [
        # All incoming messages=100, which arrives in one sec where every PROGRAM field is the same. From same PROGRAM fields we accept 100 in one sec. At the end we will have 100 matched and 0 not matched messages.
        (100, 100, 1, 100, 100, 0),
        # All incoming messages=100, which arrives in one sec where every PROGRAM field is the same. From same PROGRAM fields we accept only 1 in one sec. At the end we will have 1 matched and 99 not matched messages.
        (100, 100, 1, 1, 1, 99),
        # All incoming messages=100, which arrives in one sec where we have 5 different PROGRAM fields. From same PROGRAM fields we accept only 1 in one sec. At the end we will have 5 matched and 95 not matched messages.
        (100, 100, 5, 1, 5, 95),
        # All incoming messages=100, which arrives in one sec where we have 5 different PROGRAM fields. From same PROGRAM fields we accept 5 in one sec. At the end we will have 25 matched and 75 not matched messages.
        (100, 100, 5, 5, 25, 75),
    ], ids=["rate_limit_filter_1", "rate_limit_filter_2", "rate_limit_filter_3", "rate_limit_filter_4"],
)
def test_rate_limit_filter_acceptance(config, syslog_ng, port_allocator, bsd_formatter, message_counter, message_rate_by_sec, different_program_fields, rate_limit_rate_by_sec, expected_number_of_matched_messages, expected_number_of_not_matched_messages):
    config.update_global_options(stats_level=3)
    s_network = config.create_network_source(ip="localhost", port=port_allocator())
    f_rate_limit = config.create_rate_limit_filter(template="'${PROGRAM}'", rate=rate_limit_rate_by_sec)
    d_file = config.create_file_destination(file_name="output.log")
    config.create_logpath(statements=[s_network, f_rate_limit, d_file])

    syslog_ng.start(config)

    input_messages = generate_messages_with_different_program_fields(bsd_formatter=bsd_formatter, number_of_all_messages=message_counter, different_program_fields=different_program_fields)
    s_network.write_log(input_messages, rate=message_rate_by_sec)

    assert wait_until_true(lambda: f_rate_limit.get_stats() == {'matched': expected_number_of_matched_messages, 'not_matched': expected_number_of_not_matched_messages})
