#!/usr/bin/env python
#############################################################################
# Copyright (c) 2025 Daniele Ferla
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

def test_confgen_network_load_balancer_minimal(config, syslog_ng):
    raw_config = r"""
@include "scl.conf"

destination d_remote {
  network-load-balancer(
    targets(node1 node2 node3)
  );
};
"""
    raw_config = "@version: {}\n".format(config.get_version()) + raw_config
    config.set_raw_config(raw_config)

    syslog_ng.start(config)

def test_confgen_network_load_balancer_minimal_commas(config, syslog_ng):
    raw_config = r"""
@include "scl.conf"

destination d_remote {
  network-load-balancer(
    targets(node1, node2, node3)
  );
};
"""
    raw_config = "@version: {}\n".format(config.get_version()) + raw_config
    config.set_raw_config(raw_config)

    syslog_ng.start(config)

def test_confgen_network_load_balancer_minimal_quoted_commas(config, syslog_ng):
    raw_config = r"""
@include "scl.conf"

destination d_remote {
  network-load-balancer(
    targets("node1", "node2", "node3")
  );
};
"""
    raw_config = "@version: {}\n".format(config.get_version()) + raw_config
    config.set_raw_config(raw_config)

    syslog_ng.start(config)


def test_confgen_network_load_balancer_with_parameters(config, syslog_ng):
    raw_config = r"""
@include "scl.conf"

destination d_remote {
  network-load-balancer(
    targets("node1", "node2", "node3")
    transport(tcp)
    port(5140)
    disk-buffer(
      mem-buf-length(10000)
      disk-buf-size(10M)
      reliable(no)
      dir("/tmp/syslog-disk-buffer")
    )
  );
};
"""
    raw_config = "@version: {}\n".format(config.get_version()) + raw_config
    config.set_raw_config(raw_config)

    syslog_ng.start(config)


def test_confgen_network_load_balancer_with_custom_failover(config, syslog_ng):
    raw_config = r"""
@include "scl.conf"

destination d_remote {
  network-load-balancer(
    targets("node1", "node2", "node3")
    failover(
      failback(tcp-probe-interval(5) successful-probes-required(1))
    )
  );
};
"""
    raw_config = "@version: {}\n".format(config.get_version()) + raw_config
    config.set_raw_config(raw_config)

    syslog_ng.start(config)

def test_confgen_network_load_balancer_no_failover(config, syslog_ng):
    raw_config = r"""
@include "scl.conf"

destination d_remote {
  network-load-balancer(
    targets("node1", "node2", "node3")
    failover(off)
  );
};
"""
    raw_config = "@version: {}\n".format(config.get_version()) + raw_config
    config.set_raw_config(raw_config)

    syslog_ng.start(config)

    raw_config = r"""
@include "scl.conf"

destination d_remote {
  network-load-balancer(
    targets("node1", "node2", "node3")
    failover(no)
  );
};
"""
    raw_config = "@version: {}\n".format(config.get_version()) + raw_config
    config.set_raw_config(raw_config)

    syslog_ng.reload(config)