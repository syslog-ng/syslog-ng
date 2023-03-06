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


def test_named_logpaths_with_fallback_flag(config, port_allocator, syslog_ng, log_message, bsd_formatter):
    config.update_global_options(stats_level=1)

    source = config.create_statement_group(statements=[config.create_network_source(port=port_allocator())])

    destination_1 = config.create_statement_group(statements=[config.create_file_destination(file_name="output-1.log")])
    destination_2 = config.create_statement_group(statements=[config.create_file_destination(file_name="output-2.log")])

    filter_top_level_1 = config.create_statement_group(statements=[config.create_filter(message="'to-top-level-1'")])
    filter_top_level_2 = config.create_statement_group(statements=[config.create_filter(message="'to-top-level-2'")])
    filter_top_level_3 = config.create_statement_group(statements=[config.create_filter(message="'to-top-level-3'")])
    filter_always_false = config.create_statement_group(statements=[config.create_filter("1 == 2")])
    filter_always_true = config.create_statement_group(statements=[config.create_filter("1 == 1")])

    # log top-level-1 {
    #     source(s);
    #     filter(f_t1);
    #     log inner-1 { filter(f_false); destination(d_1); };
    #     log inner-fallback-2 { destination(d_2); flags(fallback); };
    # };
    inner_1 = config.create_inner_logpath(
        name="inner-1",
        statements=[
            filter_always_false,
            destination_1,
        ],
    )
    inner_fallback_2 = config.create_inner_logpath(
        name="inner-fallback-2",
        statements=[
            destination_2,
        ],
        flags="fallback",
    )
    top_level_1 = config.create_logpath(
        name="top-level-1",
        statements=[
            source,
            filter_top_level_1,
            inner_1,
            inner_fallback_2,
        ],
    )

    # log top-level-2 {
    #     source(s);
    #     filter(f_t2);
    #     log inner-3 { filter(f_true); destination(d_1); };
    #     log inner-fallback-4 { destination(d_2); flags(fallback); };
    # };
    inner_3 = config.create_inner_logpath(
        name="inner-3",
        statements=[
            filter_always_true,
            destination_1,
        ],
    )
    inner_fallback_4 = config.create_inner_logpath(
        name="inner-fallback-4",
        statements=[
            destination_2,
        ],
        flags="fallback",
    )
    top_level_2 = config.create_logpath(
        name="top-level-2",
        statements=[
            source,
            filter_top_level_2,
            inner_3,
            inner_fallback_4,
        ],
    )

    # log top-level-3 {
    #     source(s);
    #     filter(f_t3);
    #     log inner-5 { filter(f_false); destination(d_1); };
    #     log inner-fallback-6 { destination(d_2); flags(fallback); };
    # };
    inner_5 = config.create_inner_logpath(
        name="inner-5",
        statements=[
            filter_always_false,
            destination_1,
        ],
    )
    inner_fallback_6 = config.create_inner_logpath(
        name="inner-fallback-6",
        statements=[
            destination_2,
        ],
        flags="fallback",
    )
    top_level_3 = config.create_logpath(
        name="top-level-3",
        statements=[
            source,
            filter_top_level_3,
            destination_1,
            inner_5,
            inner_fallback_6,
        ],
    )

    logpaths = {
        "top-level-1": ExpectedLogPathStats(top_level_1, 0, 0),
        "inner-1": ExpectedLogPathStats(inner_1, 0, 0),
        "inner-fallback-2": ExpectedLogPathStats(inner_fallback_2, 0, 0),
        "top-level-2": ExpectedLogPathStats(top_level_2, 0, 0),
        "inner-3": ExpectedLogPathStats(inner_3, 0, 0),
        "inner-fallback-4": ExpectedLogPathStats(inner_fallback_4, 0, 0),
        "top-level-3": ExpectedLogPathStats(top_level_3, 0, 0),
        "inner-5": ExpectedLogPathStats(inner_5, 0, 0),
        "inner-fallback-6": ExpectedLogPathStats(inner_fallback_6, 0, 0),
    }

    syslog_ng.start(config)
    assert_multiple_logpath_stats(logpaths)

    source[0].write_log(bsd_formatter.format_message(log_message.message("to-top-level-1")))
    logpaths["top-level-1"].ingress += 1
    logpaths["top-level-1"].egress += 1
    logpaths["inner-1"].ingress += 1
    logpaths["top-level-2"].ingress += 1
    logpaths["inner-fallback-2"].ingress += 1
    logpaths["inner-fallback-2"].egress += 1
    logpaths["top-level-3"].ingress += 1
    destination_2[0].read_until_logs(["to-top-level-1"])
    assert_multiple_logpath_stats(logpaths)

    source[0].write_log(bsd_formatter.format_message(log_message.message("to-top-level-2")))
    logpaths["top-level-1"].ingress += 1
    logpaths["top-level-2"].ingress += 1
    logpaths["top-level-2"].egress += 1
    logpaths["inner-3"].ingress += 1
    logpaths["inner-3"].egress += 1
    logpaths["top-level-3"].ingress += 1
    destination_1[0].read_until_logs(["to-top-level-2"])
    assert_multiple_logpath_stats(logpaths)

    source[0].write_log(bsd_formatter.format_message(log_message.message("to-top-level-3")))
    logpaths["top-level-1"].ingress += 1
    logpaths["top-level-2"].ingress += 1
    logpaths["top-level-3"].ingress += 1
    logpaths["top-level-3"].egress += 1
    logpaths["inner-5"].ingress += 1
    logpaths["inner-fallback-6"].ingress += 1
    logpaths["inner-fallback-6"].egress += 1
    destination_1[0].read_until_logs(["to-top-level-3"])
    assert_multiple_logpath_stats(logpaths)
