#!/usr/bin/env python
#############################################################################
# Copyright (c) 2023 Attila Szakacs
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
from . import assert_multiple_logpath_stats
from . import ExpectedLogPathStats


def test_named_logpaths(config, port_allocator, syslog_ng, log_message, bsd_formatter):
    config.update_global_options(stats_level=1)

    source = config.create_statement_group(statements=[config.create_network_source(port=port_allocator())])

    destination_1 = config.create_statement_group(statements=[config.create_file_destination(file_name="output-1.log")])
    destination_2 = config.create_statement_group(statements=[config.create_file_destination(file_name="output-2.log")])

    filter_top_level_2 = config.create_statement_group(statements=[config.create_filter(message="'to-top-level-2'")])
    filter_top_level_3 = config.create_statement_group(statements=[config.create_filter(message="'to-top-level-3'")])
    filter_inner_1 = config.create_statement_group(statements=[config.create_filter(message="'to-inner-1'")])
    filter_inner_2 = config.create_statement_group(statements=[config.create_filter(message="'to-inner-2'")])
    filter_inner_3 = config.create_statement_group(statements=[config.create_filter(message="'to-inner-3'")])

    # log top-level-1 {
    #     source(s);
    # };
    top_level_1 = config.create_logpath(
        name="top-level-1",
        statements=[
            source,
        ],
    )

    # log top-level-2 {
    #     source(s);
    #     filter(f_t2);
    #     destination(d_1);
    # };
    top_level_2 = config.create_logpath(
        name="top-level-2",
        statements=[
            source,
            filter_top_level_2,
            destination_1,
        ],
    )

    # log top-level-3 {
    #     source(s);
    #     filter(f_t3);
    #     destination(d_1);
    #     destination(d_2);
    # };
    top_level_3 = config.create_logpath(
        name="top-level-3",
        statements=[
            source,
            filter_top_level_3,
            destination_1,
            destination_2,
        ],
    )

    # log top-level-4 {
    #     source(s);
    #     log inner-1 { filter(f_i1); destination(d_1); };
    #     log inner-2 { filter(f_i2); destination(d_2); };
    # };
    inner_1 = config.create_inner_logpath(
        name="inner-1",
        statements=[
            filter_inner_1,
            destination_1,
        ],
    )
    inner_2 = config.create_inner_logpath(
        name="inner-2",
        statements=[
            filter_inner_2,
            destination_2,
        ],
    )
    top_level_4 = config.create_logpath(
        name="top-level-4",
        statements=[
            source,
            inner_1,
            inner_2,
        ],
    )

    # log top-level-5 {
    #     source(s);
    #     destination(d_1);
    #     log inner-3 { filter(f_i3); destination(d_2); };
    # };
    #
    # Queueing a message to d_1, but filtering it out from inner-3 will not increase the egress counter of
    # top-level-5, as the message is not "matched" throughout the whole logpath. This is intended, so we
    # work logically with parallel log paths and different flags, like final and fallback.
    #
    # Here, destination_2 can be thought as a debugging destination. If it is a real one, the config should
    # be adjusted like this, where the top-level-5 egress will be increased, when it queues to d_1:
    # log top-level-5 { source(s); log { destination(d_1); }; log inner-3 { filter(f_i3); destination(d_2); }; };
    inner_3 = config.create_inner_logpath(
        name="inner-3",
        statements=[
            filter_inner_3,
            destination_2,
        ],
    )
    top_level_5 = config.create_logpath(
        name="top-level-5",
        statements=[
            source,
            destination_1,
            inner_3,
        ],
    )

    logpaths = {
        "top-level-1": ExpectedLogPathStats(top_level_1, 0, 0),
        "top-level-2": ExpectedLogPathStats(top_level_2, 0, 0),
        "top-level-3": ExpectedLogPathStats(top_level_3, 0, 0),
        "top-level-4": ExpectedLogPathStats(top_level_4, 0, 0),
        "inner-1": ExpectedLogPathStats(inner_1, 0, 0),
        "inner-2": ExpectedLogPathStats(inner_2, 0, 0),
        "top-level-5": ExpectedLogPathStats(top_level_5, 0, 0),
        "inner-3": ExpectedLogPathStats(inner_3, 0, 0),
    }

    syslog_ng.start(config)
    assert_multiple_logpath_stats(logpaths)

    source[0].write_log(bsd_formatter.format_message(log_message.message("to-top-level-2")))
    logpaths["top-level-1"].ingress += 1
    logpaths["top-level-1"].egress += 1
    logpaths["top-level-2"].ingress += 1
    logpaths["top-level-2"].egress += 1
    logpaths["top-level-3"].ingress += 1
    logpaths["top-level-4"].ingress += 1
    logpaths["inner-1"].ingress += 1
    logpaths["inner-2"].ingress += 1
    logpaths["top-level-5"].ingress += 1
    logpaths["inner-3"].ingress += 1
    destination_1[0].read_until_logs(["to-top-level-2"])
    assert_multiple_logpath_stats(logpaths)

    source[0].write_log(bsd_formatter.format_message(log_message.message("to-top-level-3")))
    logpaths["top-level-1"].ingress += 1
    logpaths["top-level-1"].egress += 1
    logpaths["top-level-2"].ingress += 1
    logpaths["top-level-3"].ingress += 1
    logpaths["top-level-3"].egress += 1
    logpaths["top-level-4"].ingress += 1
    logpaths["inner-1"].ingress += 1
    logpaths["inner-2"].ingress += 1
    logpaths["top-level-5"].ingress += 1
    logpaths["inner-3"].ingress += 1
    destination_1[0].read_until_logs(["to-top-level-3"])
    destination_2[0].read_until_logs(["to-top-level-3"])
    assert_multiple_logpath_stats(logpaths)

    source[0].write_log(bsd_formatter.format_message(log_message.message("to-inner-1")))
    logpaths["top-level-1"].ingress += 1
    logpaths["top-level-1"].egress += 1
    logpaths["top-level-2"].ingress += 1
    logpaths["top-level-3"].ingress += 1
    logpaths["top-level-4"].ingress += 1
    logpaths["top-level-4"].egress += 1
    logpaths["inner-1"].ingress += 1
    logpaths["inner-1"].egress += 1
    logpaths["inner-2"].ingress += 1
    logpaths["top-level-5"].ingress += 1
    logpaths["inner-3"].ingress += 1
    destination_1[0].read_until_logs(["to-inner-1"])
    assert_multiple_logpath_stats(logpaths)

    source[0].write_log(bsd_formatter.format_message(log_message.message("to-inner-2")))
    logpaths["top-level-1"].ingress += 1
    logpaths["top-level-1"].egress += 1
    logpaths["top-level-2"].ingress += 1
    logpaths["top-level-3"].ingress += 1
    logpaths["top-level-4"].ingress += 1
    logpaths["top-level-4"].egress += 1
    logpaths["inner-1"].ingress += 1
    logpaths["inner-2"].ingress += 1
    logpaths["inner-2"].egress += 1
    logpaths["top-level-5"].ingress += 1
    logpaths["inner-3"].ingress += 1
    destination_2[0].read_until_logs(["to-inner-2"])
    assert_multiple_logpath_stats(logpaths)

    source[0].write_log(bsd_formatter.format_message(log_message.message("to-inner-3")))
    logpaths["top-level-1"].ingress += 1
    logpaths["top-level-1"].egress += 1
    logpaths["top-level-2"].ingress += 1
    logpaths["top-level-3"].ingress += 1
    logpaths["top-level-4"].ingress += 1
    logpaths["inner-1"].ingress += 1
    logpaths["inner-2"].ingress += 1
    logpaths["top-level-5"].ingress += 1
    logpaths["top-level-5"].egress += 1
    logpaths["inner-3"].ingress += 1
    logpaths["inner-3"].egress += 1
    destination_1[0].read_until_logs(["to-inner-3"])
    destination_2[0].read_until_logs(["to-inner-3"])
    assert_multiple_logpath_stats(logpaths)
