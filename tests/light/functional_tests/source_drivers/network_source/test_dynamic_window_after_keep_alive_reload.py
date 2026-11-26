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

import pytest

from src.common.blocking import wait_until_true

STATIC_WINDOW = 100  # log-iw-size(1000) / max-connections(10)
OLD_PEER, NEW_PEER, THIRD_PEER = "127.0.0.2", "127.0.0.3", "127.0.0.4"


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


def _send(client, text):
    client.sendall(f"<34>Sep 28 10:00:00 host app: {text}\n".encode("ascii"))


def _source_options(port, keep_alive, so_reuseport, dynamic_window_size):
    return dict(
        ip="localhost",
        port=port,
        keep_alive=keep_alive,
        so_reuseport=so_reuseport,
        max_connections=10,
        log_iw_size=1000,
        dynamic_window_size=dynamic_window_size,
        dynamic_window_stats_freq=0.05,
        dynamic_window_realloc_ticks=1,
    )


def test_reload_closes_connections_the_new_config_does_not_keep(config, syslog_ng, syslog_ng_ctl, port_allocator):
    # keep-alive(yes) -> keep-alive(no) with so-reuseport(yes): the new configuration does not take the
    # old connections over. They used to stay in its connection list uninitialized, without a dynamic
    # window pool, and the realloc timer dereferenced the missing pool (SIGSEGV).
    config.update_global_options(stats_level=4)
    port = port_allocator()
    source = config.create_network_source(**_source_options(port, "yes", "yes", 0))
    destination = config.create_file_destination(file_name="output.log", template=r'"${MESSAGE}\n"')
    config.create_logpath(statements=[source, destination])
    syslog_ng.start(config)

    old_client = socket.create_connection(("127.0.0.1", port))
    try:
        _send(old_client, "before-reload")
        assert destination.read_log() == "before-reload\n"

        source.options["keep_alive"] = "no"
        source.options["dynamic_window_size"] = 10000
        syslog_ng.reload(config)

        # not kept alive: the old connection is closed once the reload is done
        old_client.settimeout(5)
        assert old_client.recv(1) == b""

        new_client = socket.create_connection(("127.0.0.1", port))
        try:
            _send(new_client, "after-reload")
            assert destination.read_log() == "after-reload\n"
            assert wait_until_true(lambda: any(w > STATIC_WINDOW for w in _full_windows(syslog_ng_ctl)))
            assert syslog_ng.is_process_running()
        finally:
            new_client.close()
    finally:
        old_client.close()


@pytest.mark.parametrize(
    "new_keep_alive, so_reuseport",
    [("yes", "yes"), ("yes", "no"), ("no", "no")],
    ids=["keep-alive-reuseport", "keep-alive", "restored-without-reuseport"],
)
def test_restored_connection_joins_the_pool_of_a_reload_that_enables_dynamic_window(
    config, syslog_ng, syslog_ng_ctl, port_allocator, new_keep_alive, so_reuseport,
):
    # The old connection is restored (keep-alive(yes), or a reload without so-reuseport(), which restores
    # it regardless of keep-alive()) into a configuration that turns dynamic-window-size() on. The restored
    # reader had no pool and was never given one; the realloc timer dereferenced it (SIGSEGV).
    config.update_global_options(stats_level=4)
    port = port_allocator()
    source = config.create_network_source(**_source_options(port, "yes", so_reuseport, 0))
    destination = config.create_file_destination(file_name="output.log", template=r'"${MESSAGE}\n"')
    config.create_logpath(statements=[source, destination])
    syslog_ng.start(config)

    old_client = socket.create_connection(("127.0.0.1", port))
    try:
        _send(old_client, "before-reload")
        assert destination.read_log() == "before-reload\n"

        source.options["keep_alive"] = new_keep_alive
        source.options["dynamic_window_size"] = 10000
        syslog_ng.reload(config)

        new_client = socket.create_connection(("127.0.0.1", port))
        try:
            _send(new_client, "after-reload-new")
            assert destination.read_log() == "after-reload-new\n"
            _send(old_client, "after-reload-old")
            assert destination.read_log() == "after-reload-old\n"
            # both connections, the restored one included, get a dynamic share above the static window
            assert wait_until_true(lambda: sum(1 for w in _full_windows(syslog_ng_ctl) if w > STATIC_WINDOW) >= 2)
            assert syslog_ng.is_process_running()
        finally:
            new_client.close()
    finally:
        old_client.close()


def _connect_from(peer, port):
    # full_window is labeled with the address of the peer, not with its port
    return socket.create_connection(("127.0.0.1", port), source_address=(peer, 0))


def _peer_full_windows(syslog_ng_ctl):
    windows = {}
    for line in syslog_ng_ctl.query(pattern="*full_window*")["stdout"].splitlines():
        name, _, value = line.rpartition("=")
        for peer in (OLD_PEER, NEW_PEER, THIRD_PEER):
            if peer in name:
                windows[peer] = int(value)
    return windows


def _wait_for_windows(syslog_ng_ctl, expected):
    def matches():
        windows = _peer_full_windows(syslog_ng_ctl)
        return all(windows.get(peer) == window for peer, window in expected.items())

    wait_until_true(matches)
    assert {peer: _peer_full_windows(syslog_ng_ctl).get(peer) for peer in expected} == expected


@pytest.mark.parametrize(
    "old_size, new_keep_alive, so_reuseport, new_size, expected_window",
    [
        (10000, "no", "yes", 10000, 5100),
        (10000, "no", "yes", 0, 5100),
        (10000, "no", "yes", 20000, 5100),
        (0, "no", "yes", 10000, STATIC_WINDOW),
        (10000, "yes", "yes", 10000, 5100),
        (10000, "yes", "no", 10000, 5100),
    ],
    ids=[
        "not-taken-over", "not-taken-over-size-off", "not-taken-over-size-changed", "not-taken-over-size-on",
        "taken-over", "taken-over-no-reuseport",
    ],
)
def test_reverted_reload_hands_the_connections_back_with_their_pool(
    config, syslog_ng, syslog_ng_ctl, port_allocator, old_size, new_keep_alive, so_reuseport, new_size, expected_window,
):
    # The first source of the new configuration initializes, the second one fails, and syslog-ng reverts to
    # the old configuration (keep-alive(yes)). The old connection must be restored, not closed: the failed
    # configuration must not take it out of the persist config and kill it on its way out. Its pool must
    # come back with it: the reader keeps allocating from the pool it has, while new connections and the
    # rebalancing use the pool of the driver, so two pools would let the readers exceed dynamic-window-size().
    config.update_global_options(stats_level=4)
    port, busy_port = port_allocator(), port_allocator()
    source = config.create_network_source(**_source_options(port, "yes", so_reuseport, old_size))
    destination = config.create_file_destination(file_name="output.log", template=r'"${MESSAGE}\n"')
    config.create_logpath(statements=[source, destination])
    syslog_ng.start(config)

    old_client = _connect_from(OLD_PEER, port)
    blocker = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        _send(old_client, "before-reload")
        assert destination.read_log() == "before-reload\n"
        _wait_for_windows(syslog_ng_ctl, {OLD_PEER: old_size + STATIC_WINDOW})

        # the new configuration fails to initialize: its second source cannot bind a port held here
        blocker.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        blocker.bind(("127.0.0.1", busy_port))
        blocker.listen(1)
        source.options["keep_alive"] = new_keep_alive
        source.options["dynamic_window_size"] = new_size
        failing_source = config.create_network_source(ip="localhost", port=busy_port)
        config.create_logpath(statements=[failing_source, destination])
        config.write_config(syslog_ng.instance_paths.get_config_path())
        assert syslog_ng_ctl.reload()["exit_code"] != 0
        assert syslog_ng.is_process_running()

        # reverted: the old connection is still served, and it shares its budget with a new one
        _send(old_client, "after-failed-reload")
        assert destination.read_log() == "after-failed-reload\n"

        new_client = _connect_from(NEW_PEER, port)
        try:
            _send(new_client, "new-client")
            assert destination.read_log() == "new-client\n"
            _wait_for_windows(syslog_ng_ctl, {OLD_PEER: expected_window, NEW_PEER: expected_window})
            assert syslog_ng.is_process_running()
        finally:
            new_client.close()
    finally:
        blocker.close()
        old_client.close()


@pytest.mark.parametrize("new_size", [10000, 0, 20000], ids=["same-size", "size-off", "size-changed"])
def test_completed_reload_releases_the_pool_it_does_not_take_over(config, syslog_ng, syslog_ng_ctl, port_allocator, new_size):
    # The negative twin of the reverted reload: when it completes, the old connection is closed, its pool is
    # released with it, and the new connections share the budget of the new configuration alone.
    config.update_global_options(stats_level=4)
    port = port_allocator()
    source = config.create_network_source(**_source_options(port, "yes", "yes", 10000))
    destination = config.create_file_destination(file_name="output.log", template=r'"${MESSAGE}\n"')
    config.create_logpath(statements=[source, destination])
    syslog_ng.start(config)

    old_client = _connect_from(OLD_PEER, port)
    try:
        _send(old_client, "before-reload")
        assert destination.read_log() == "before-reload\n"
        _wait_for_windows(syslog_ng_ctl, {OLD_PEER: 10000 + STATIC_WINDOW})

        source.options["keep_alive"] = "no"
        source.options["dynamic_window_size"] = new_size
        syslog_ng.reload(config)

        old_client.settimeout(5)
        assert old_client.recv(1) == b""

        new_client = _connect_from(NEW_PEER, port)
        third_client = _connect_from(THIRD_PEER, port)
        try:
            share = new_size // 2 + STATIC_WINDOW
            _wait_for_windows(syslog_ng_ctl, {NEW_PEER: share, THIRD_PEER: share})
            assert syslog_ng.is_process_running()
        finally:
            third_client.close()
            new_client.close()
    finally:
        old_client.close()
