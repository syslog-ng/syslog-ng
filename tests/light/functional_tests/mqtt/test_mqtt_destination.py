#!/usr/bin/env python
#############################################################################
# Copyright (c) 2022 One Identity
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
from src.driver_io.mqtt_broker.mqtt_broker import MQTTBroker
from src.driver_io.mqtt_broker.mqtt_config import MQTTConfig


def test_mqtt_destination(config, syslog_ng):
    mqtt_config = MQTTConfig(topic="syslogng/test/destination")
    broker = MQTTBroker(mqtt_config)

    generator_source = config.create_example_msg_generator_source(num=1)

    mqtt_destination = config.create_mqtt_destination(mqtt_config, template="'$MSG\n'")
    config.create_logpath(statements=[generator_source, mqtt_destination])

    broker.start()
    print(syslog_ng.get_version())
    syslog_ng.start(config)

    log = str(mqtt_destination.reciveMessage().payload, "utf-8")
    assert log == generator_source.DEFAULT_MESSAGE + "\n"

    broker.stop()
