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


def test_named_logpaths_with_catchall_flag(config, port_allocator, syslog_ng, log_message, bsd_formatter):
    config.update_global_options(stats_level=1)

    source = config.create_statement_group(statements=[config.create_network_source(port=port_allocator())])

    destination = config.create_statement_group(statements=[config.create_file_destination(file_name="output.log")])

    filter_top_level_1 = config.create_statement_group(statements=[config.create_filter(message="'to-top-level-1'")])
    filter_top_level_2 = config.create_statement_group(statements=[config.create_filter(message="'to-top-level-2'")])

    # log top-level-1 {
    #     filter(f_t1);
    #     destination(d);
    #     flags(catchall);
    # };
    top_level_1 = config.create_logpath(
        name="top-level-1",
        statements=[
            filter_top_level_1,
            destination,
        ],
        flags="catchall",
    )

    # log top-level-2 {
    #     filter(f_t2);
    #     destination(d);
    #     flags(catchall);
    # };
    top_level_2 = config.create_logpath(
        name="top-level-2",
        statements=[
            filter_top_level_2,
            destination,
        ],
        flags="catchall",
    )

    logpaths = {
        "top-level-1": ExpectedLogPathStats(top_level_1, 0, 0),
        "top-level-2": ExpectedLogPathStats(top_level_2, 0, 0),
    }

    syslog_ng.start(config)
    assert_multiple_logpath_stats(logpaths)

    source[0].write_log(bsd_formatter.format_message(log_message.message("to-top-level-1")))
    logpaths["top-level-1"].ingress += 1
    logpaths["top-level-1"].egress += 1
    logpaths["top-level-2"].ingress += 1
    destination[0].read_until_logs(["to-top-level-1"])
    assert_multiple_logpath_stats(logpaths)

    source[0].write_log(bsd_formatter.format_message(log_message.message("to-top-level-2")))
    logpaths["top-level-1"].ingress += 1
    logpaths["top-level-2"].ingress += 1
    logpaths["top-level-2"].egress += 1
    destination[0].read_until_logs(["to-top-level-2"])
    assert_multiple_logpath_stats(logpaths)
