#!/usr/bin/env python
#############################################################################
# Copyright (c) 2024 Balazs Scheidler <balazs.scheidler@axoflow.com>
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
from src.syslog_ng_ctl.prometheus_stats_handler import MetricFilter


def test_metrics_probe(config, syslog_ng):
    config.update_global_options(stats_level=1)
    generator_source = config.create_example_msg_generator_source(num=1, template=config.stringify("message"), values="dim1 => value1 dim2 => value2")

    probe1 = config.create_metrics_probe(key="test_metric1", labels=config.arrowed_options({"label1": config.stringify("$dim1")}))
    probe2 = config.create_metrics_probe(key="test_metric2", labels=config.arrowed_options({"label2": config.stringify("$dim2")}))

    file_destination = config.create_file_destination(file_name="output.log", template=config.stringify("$MSG\n"))
    config.create_logpath(statements=[generator_source, probe1, probe2, file_destination])

    syslog_ng.start(config)

    # message goes through
    assert file_destination.read_log().strip() == "message"

    samples = config.get_prometheus_samples([MetricFilter("syslogng_test_metric1", {})])
    assert len(samples) > 0
    assert samples[0].labels == {'label1': 'value1'}

    samples = config.get_prometheus_samples([MetricFilter("syslogng_test_metric2", {})])
    assert len(samples) > 0
    assert samples[0].labels == {'label2': 'value2'}
