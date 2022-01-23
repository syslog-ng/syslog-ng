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
from pathlib2 import Path

import src.testcase_parameters.testcase_parameters as tc_parameters
from src.executors.command_executor import CommandExecutor


class MQTTConfig():

    def __setDefault(self):
        self.options["topic"] = self.options["topic"] if "topic" in self.options else "syslog-ng"
        self.options["qos"] = self.options["qos"] if "qos" in self.options else 2
        self.options["hostname"] = self.options["hostname"] if "hostname" in self.options else "localhost"
        self.options["port"] = self.options["port"] if "port" in self.options else 11111
        self.options["keepalive"] = self.options["keepalive"] if "keepalive" in self.options else 60
        self.options["protocol"] = self.options["protocol"] if "protocol" in self.options else "tcp"

    def __init__(self, **options):
        self.options = options
        self.__setDefault()
        pass

    def getHostname(self):
        return self.options["hostname"] if "hostname" in self.options else None

    def getPort(self):
        return self.options["port"] if "port" in self.options else None

    def getKeepalive(self):
        return self.options["keepalive"] if "keepalive" in self.options else None

    def getWill(self):
        return self.options["will"] if "will" in self.options else None

    def getUsername(self):
        return self.options["username"] if "username" in self.options else None

    def getPassword(self):
        return self.options["password"] if "password" in self.options else None

    def getTopic(self):
        return self.options["topic"] if "topic" in self.options else None

    def getQos(self):
        return self.options["qos"] if "qos" in self.options else None

    def getAuth(self):
        result = {}

        if "username" in self.options:
            result.update({"username": self.options["username"]})

            if "password" in self.options:
                result.update({"password": self.options["password"]})

        if not bool(result):
            return None

        return result

    def getTLS(self):
        return None

    def getProtocol(self):
        return self.options["protocol"] if "protocol" in self.options else None

    def generateAddress(self):
        return "{protocol}://{hostname}:{port}".format(protocol=self.getProtocol(), hostname=self.getHostname(), port=self.getPort())

    def __generateMQTTConfigTopic(self):
        result = ""
        return result

    def __generateMQTTConfigAddress(self):
        result = ""
        result += "listener {port}\n".format(port=self.getPort())
        return result

    def __generatePasswordFile(self):
        if self.getUsername() is None:
            return (False, "")

        path = Path(tc_parameters.WORKING_DIR, "mosquitto_config")

        passwd_out = Path(tc_parameters.WORKING_DIR, "passwd_out")
        passwd_err = Path(tc_parameters.WORKING_DIR, "passwd_err")

        cmd = ["mosquitto_passwd", "-m", path, self.getUsername()]
        if self.getPassword():
            cmd.append(self.getPassword())

        result = CommandExecutor().run(
            cmd, passwd_out, passwd_err,
        )

        return (result["exit_code"] == 0, path)

    def __generateMQTTConfigAuth(self):
        success, path = self.__generatePasswordFile()
        if not success:
            return "allow_anonymous true \n"

        result = "allow_anonymous false\n"
        result += "password_file {path}\n".format(path)

        return result

    # TODO
    def __generateMQTTConfigTLS(self):
        return ""

    def generateMQTTConfig(self):
        result = ""
        result += self.__generateMQTTConfigTopic() + "\n"
        result += self.__generateMQTTConfigAddress() + "\n"
        result += self.__generateMQTTConfigAuth() + "\n"
        result += self.__generateMQTTConfigTLS() + "\n"
        return result

    def __generateSyslogTopic(self):
        return {"topic": self.getTopic()}

    def __generateSyslogQos(self):
        return {"qos": self.getQos()}

    def __generateSyslogAddress(self):
        address = "\"{address}\"".format(address=self.generateAddress())
        return {"address": address}

    def __generateSyslogAuth(self):
        if self.getAuth() is None:
            return {}

        result = {}

        if self.getUsername():
            result.update({"username": self.getUsername()})
            if self.getPassword():
                result.update({"password": self.getPassword()})

        return result

    # TODO
    def __generateSyslogTLS(self):
        return {}

    def generateSyslogConfig(self):
        result = {}
        result.update(self.__generateSyslogTopic())
        result.update(self.__generateSyslogQos())
        result.update(self.__generateSyslogAddress())
        result.update(self.__generateSyslogAuth())
        result.update(self.__generateSyslogTLS())
        return result
