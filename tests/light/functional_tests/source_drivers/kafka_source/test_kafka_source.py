#!/usr/bin/env python
#############################################################################
# Copyright (c) 2026 One Identity
# Copyright (c) 2026 Hofi <hofione@gmail.com>
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
"""
Light tests for the kafka() source driver.

These exercise scenarios the Criterion unit tests in
modules/kafka/tests/test_kafka_source.c cannot cover:

  - the full Bison grammar path (kafka() source syntax)
  - the LogThreadedSourceDriver init/deinit lifecycle with a real main loop
  - mandatory-option enforcement via the actual daemon startup
  - librdkafka client construction without crashing the daemon when the
    broker is unreachable

A reachable broker is NOT required. librdkafka accepts the broker address
at startup and only retries asynchronously, so the daemon stays up. Tests
that need end-to-end message flow belong in a separate broker-backed tier.
"""
import time

import pytest


# A locally-unreachable bootstrap-servers value. The TCP connection attempt
# returns ECONNREFUSED immediately and librdkafka enters its retry loop,
# but the daemon comes up successfully — the only behaviour we need for
# config-level light tests.
UNREACHABLE_BROKER = "127.0.0.1:1"


def _kafka_source_config(version, *, topic_clause=None, bootstrap=UNREACHABLE_BROKER, extra=""):
    """Build a syslog-ng config snippet using kafka() as a source.

    Each option is added on its own line only when supplied so the negative
    tests can omit topic() / bootstrap-servers() individually.
    """
    lines = []
    if topic_clause:
        lines.append(f"    {topic_clause}")
    if bootstrap:
        lines.append(f'    bootstrap-servers("{bootstrap}")')
    if extra:
        lines.append(f"    {extra}")
    kafka_body = "\n".join(lines)

    return f"""@version: {version}

source s_kafka {{
  kafka(
{kafka_body}
  );
}};

destination d_file {{
  file("output.log");
}};

log {{
  source(s_kafka);
  destination(d_file);
}};
"""


# ---------------------------------------------------------------------------
#  Mandatory option enforcement (negative tests)
# ---------------------------------------------------------------------------

def test_topic_is_mandatory(config, syslog_ng):
    """kafka() source without topic() must fail to start."""
    config.set_raw_config(
        _kafka_source_config(
            config.get_version(),
            topic_clause=None,
            bootstrap=UNREACHABLE_BROKER,
        ),
    )

    with pytest.raises(Exception):
        syslog_ng.start(config)


def test_bootstrap_servers_is_mandatory(config, syslog_ng):
    """kafka() source without bootstrap-servers() must fail to start."""
    config.set_raw_config(
        _kafka_source_config(
            config.get_version(),
            topic_clause='topic("my-topic" => "0")',
            bootstrap=None,
        ),
    )

    with pytest.raises(Exception):
        syslog_ng.start(config)


# ---------------------------------------------------------------------------
#  Full lifecycle
# ---------------------------------------------------------------------------

def test_clean_startup_and_shutdown(config, syslog_ng):
    """Happy-path init + the full LogThreadedSourceDriver teardown.

    test_kafka_source.c omits this scenario because Criterion has no main
    loop to join the librdkafka background threads on; the daemon does, so
    we can verify start->stop here.
    """
    config.set_raw_config(
        _kafka_source_config(
            config.get_version(),
            topic_clause='topic("my-topic" => "0")',
        ),
    )

    syslog_ng.start(config)
    syslog_ng.stop()


# ---------------------------------------------------------------------------
#  Offline broker - retry / reconnect behaviour
#
#  These tests run with an unreachable broker so the daemon's reconnect
#  paths are exercised. They guard against:
#    - the daemon aborting when librdkafka's background thread reports
#      connection errors via _kafka_error_cb
#    - shutdown hangs while the reconnect loop is active
#    - crashes in _restore_msg_offsets_from_remote when rd_kafka_committed
#      times out against an offline broker
#    - the assign vs subscribe strategy diverging on offline behaviour
#
#  Each test sleeps briefly so the librdkafka background thread has time
#  to attempt (and fail) at least one connect before we tear down.
# ---------------------------------------------------------------------------

# Long enough for librdkafka's background thread to attempt and fail at
# least one TCP connect; short enough to keep the test suite snappy.
OFFLINE_RETRY_SETTLE_SECONDS = 2


def test_offline_broker_survives_brief_run(config, syslog_ng):
    """Daemon must survive a couple of seconds against an unreachable broker
    and then stop cleanly. Catches crashes in _kafka_error_cb /
    _kafka_log_state_changed during the retry storm."""
    config.set_raw_config(
        _kafka_source_config(
            config.get_version(),
            topic_clause='topic("my-topic" => "0")',
            bootstrap=UNREACHABLE_BROKER,
        ),
    )

    syslog_ng.start(config)
    time.sleep(OFFLINE_RETRY_SETTLE_SECONDS)
    syslog_ng.stop()


def test_multiple_offline_brokers_in_csv_survive(config, syslog_ng):
    """A CSV list of unreachable brokers must be parsed and probed without
    crashing. Catches issues in librdkafka's multi-broker fail-over loop and
    in syslog-ng's bootstrap-servers handling."""
    brokers_csv = "127.0.0.1:1,127.0.0.1:2,127.0.0.1:3"
    config.set_raw_config(
        _kafka_source_config(
            config.get_version(),
            topic_clause='topic("my-topic" => "0")',
            bootstrap=brokers_csv,
        ),
    )

    syslog_ng.start(config)
    time.sleep(OFFLINE_RETRY_SETTLE_SECONDS)
    syslog_ng.stop()


def test_dns_unresolvable_broker_survives(config, syslog_ng):
    """librdkafka raises a different error path for DNS failures than for
    ECONNREFUSED. The daemon must survive both. Uses an RFC 6761 reserved
    .invalid TLD so no DNS lookup ever succeeds anywhere."""
    config.set_raw_config(
        _kafka_source_config(
            config.get_version(),
            topic_clause='topic("my-topic" => "0")',
            bootstrap="kafka-does-not-exist.invalid:9092",
        ),
    )

    syslog_ng.start(config)
    time.sleep(OFFLINE_RETRY_SETTLE_SECONDS)
    syslog_ng.stop()


def test_rapid_start_stop_offline(config, syslog_ng):
    """Stop the daemon immediately during the initial connect-retry
    cycle. Guards against shutdown deadlocks when the librdkafka background
    thread is still trying to dial the broker."""
    config.set_raw_config(
        _kafka_source_config(
            config.get_version(),
            topic_clause='topic("my-topic" => "0")',
            bootstrap=UNREACHABLE_BROKER,
        ),
    )

    syslog_ng.start(config)
    syslog_ng.stop()


def test_remote_persist_with_offline_broker(config, syslog_ng):
    """persist-store("remote") routes offset restoration through
    rd_kafka_committed, which times out against an offline broker. The
    daemon must log the failure and continue, not crash."""
    config.set_raw_config(
        _kafka_source_config(
            config.get_version(),
            topic_clause='topic("my-topic" => "0")',
            bootstrap=UNREACHABLE_BROKER,
            extra='persist-store("remote")',
        ),
    )

    syslog_ng.start(config)
    time.sleep(OFFLINE_RETRY_SETTLE_SECONDS)
    syslog_ng.stop()


@pytest.mark.parametrize("strategy_hint", ["subscribe", "assign"])
def test_offline_broker_both_strategies(config, syslog_ng, strategy_hint):
    """The assign and subscribe strategies take different code paths in
    _setup_kafka_client. Both must tolerate an offline broker: assign never
    completes the partition assignment, subscribe never gets a rebalance
    callback, and both must still allow a clean shutdown."""
    config.set_raw_config(
        _kafka_source_config(
            config.get_version(),
            topic_clause='topic("my-topic" => "-1")',
            bootstrap=UNREACHABLE_BROKER,
            extra=f'strategy-hint("{strategy_hint}")',
        ),
    )

    syslog_ng.start(config)
    time.sleep(OFFLINE_RETRY_SETTLE_SECONDS)
    syslog_ng.stop()


def test_short_time_reopen_offline(config, syslog_ng):
    """A 1-second time-reopen() forces the worker to attempt a full
    kafka_sd_reopen (destroy + reconstruct of the rd_kafka_t client) several
    times during the test window. Guards against leaks and crashes in the
    reopen path under offline conditions."""
    config.set_raw_config(
        _kafka_source_config(
            config.get_version(),
            topic_clause='topic("my-topic" => "0")',
            bootstrap=UNREACHABLE_BROKER,
            extra='time-reopen(1)',
        ),
    )

    syslog_ng.start(config)
    # Long enough for ~3 reopen cycles at time-reopen(1).
    time.sleep(4)
    syslog_ng.stop()


# ---------------------------------------------------------------------------
#  rd_kafka_conf_t leak fix - construct-failure path
# ---------------------------------------------------------------------------

def test_invalid_config_property_fails_to_start(config, syslog_ng):
    """An unknown librdkafka property forces kafka_apply_config_props() to
    fail inside _construct_kafka_client(), driving the function through its
    err_exit path. Pins the rd_kafka_conf_destroy(conf) leak fix added to
    every early-return branch of _construct_kafka_client / _construct_client.

    The happy-path tests can never reach these branches: librdkafka tolerates
    an offline broker at startup, so rd_kafka_new() succeeds and conf is
    consumed normally. Only a config-validation failure exercises the leak
    paths."""
    config.set_raw_config(
        _kafka_source_config(
            config.get_version(),
            topic_clause='topic("my-topic" => "0")',
            bootstrap=UNREACHABLE_BROKER,
            extra='config("totally.invalid.librdkafka.property" => "x")',
        ),
    )

    with pytest.raises(Exception):
        syslog_ng.start(config)
