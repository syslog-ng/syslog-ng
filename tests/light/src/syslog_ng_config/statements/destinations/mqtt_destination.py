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
import paho.mqtt.subscribe as subscribe

from src.syslog_ng_config.statements.destinations.destination_driver import DestinationDriver


class MQTTDestination(DestinationDriver):
    def __init__(self, mqtt_config, **options):
        self.mqtt_config = mqtt_config

        options.update(self.generateConfig())
        self.driver_name = "mqtt"
        super(MQTTDestination, self).__init__(None, options)

    def generateAuth(self):
        return {}

    def generateTLS(self):
        return {}

    def generateConfig(self):
        result = {}
        result.update(self.mqtt_config.generateSyslogConfig())
        return result

    def reciveMessage(self):
        return subscribe.simple(topics=[self.mqtt_config.getTopic()], qos=self.mqtt_config.getQos(), msg_count=1, hostname=self.mqtt_config.getHostname(), port=self.mqtt_config.getPort(), auth=self.mqtt_config.getAuth(), tls=self.mqtt_config.getTLS())

    def getReciveString(self):
        msg = self.recivedMessage()
        return str(msg.payload, "utf-8")
