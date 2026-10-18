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
import socket

from src.common.blocking import wait_until_true

STATIC_WINDOW = 100  # log-iw-size(1000) / max-connections(10)


def _full_windows(syslog_ng_ctl):
    stdout = syslog_ng_ctl.query(pattern="*full_window*")["stdout"]
    windows = []
    for line in stdout.splitlines():
        if "full_window" in line and "=" in line:
            windows.append(int(line.rsplit("=", 1)[1]))
    return windows


def test_dynamic_window_survives_reload_that_disables_keep_alive(config, syslog_ng, syslog_ng_ctl, port_allocator):
    config.update_global_options(stats_level=4)
    port = port_allocator()
    source = config.create_network_source(
        ip="localhost",
        port=port,
        keep_alive="yes",
        so_reuseport="yes",
        max_connections=10,
        log_iw_size=1000,
        dynamic_window_size=10000,
        dynamic_window_realloc_ticks=1,
    )
    destination = config.create_file_destination(file_name="output.log")
    config.create_logpath(statements=[source, destination])
    syslog_ng.start(config)

    # the old configuration hands its dynamic window pool over to the next one (keep-alive(yes))
    source.options["keep_alive"] = "no"
    syslog_ng.reload(config)

    client = socket.create_connection(("127.0.0.1", port))
    try:
        client.sendall(b"<34>Sep 28 10:00:00 host app: after-reload\n")
        assert wait_until_true(lambda: any(w > STATIC_WINDOW for w in _full_windows(syslog_ng_ctl)))
    finally:
        client.close()
