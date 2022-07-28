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
import time

from src.driver_io.mqtt_broker.mqtt_broker import MQTTBroker
from src.driver_io.mqtt_broker.mqtt_config import MQTTConfig


def test_mqtt_source(config, syslog_ng):
    config.update_global_options(time_reopen=1)
    mqtt_config = MQTTConfig(topic="syslogng/test/source")
    broker = MQTTBroker(mqtt_config)

    mqtt_source = config.create_mqtt_source(mqtt_config)

    file_destination = config.create_file_destination(file_name="output.log", template="'$MSG\n'")
    config.create_logpath(statements=[mqtt_source, file_destination])

    broker.start()
    print(syslog_ng.get_version())
    syslog_ng.start(config)

    time.sleep(2)  # It's need, beacuse need to start up everything

    mqtt_source.sendMessage(payload="hello there")

    log = file_destination.read_log()
    assert log == "hello there\n"

    broker.stop()
