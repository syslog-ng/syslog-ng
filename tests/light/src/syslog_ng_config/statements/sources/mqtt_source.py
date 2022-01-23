#!/usr/bin/env python
#############################################################################
# Copyright (c) 2015-2019 Balabit
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
import paho.mqtt.publish as publish

from src.syslog_ng_config.statements.sources.source_driver import SourceDriver


class MQTTSource(SourceDriver):
    def __init__(self, mqtt_config, **options):
        self.mqtt_config = mqtt_config

        options.update(self.generateConfig())
        self.driver_name = "mqtt"
        super(MQTTSource, self).__init__(None, options)

    def generateConfig(self):
        result = {}
        result.update(self.mqtt_config.generateSyslogConfig())
        return result

    def sendMessage(self, payload="test"):
        publish.single(self.mqtt_config.getTopic(), payload=payload, qos=self.mqtt_config.getQos(), hostname=self.mqtt_config.getHostname(), port=self.mqtt_config.getPort(), auth=self.mqtt_config.getAuth(), tls=self.mqtt_config.getTLS())
