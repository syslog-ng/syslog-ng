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
from psutil import TimeoutExpired

import src.testcase_parameters.testcase_parameters as tc_parameters
from src.common.file import File
from src.executors.process_executor import ProcessExecutor


class MQTTBroker():
    def __init__(self, config):
        self.config = config
        self.process = None
        self.configpath = self.createConfigFile()

    def createConfigFile(self):
        path = Path(tc_parameters.WORKING_DIR, "mosquitto_config")

        f = File(path)
        with f.open('w+') as config_file:
            config_file.write(self.config.generateMQTTConfig())

        print(path.resolve())
        return path.resolve()

    def start(self):
        mqtt_broker_stdout_path = Path(tc_parameters.WORKING_DIR, "mqtt_broker_stdout")
        mqtt_broker_stderr_path = Path(tc_parameters.WORKING_DIR, "mqtt_broker_stderr")

        self.process = ProcessExecutor().start(
            command=["/usr/sbin/mosquitto", "-c", "{config_path}".format(config_path=self.configpath)],
            stdout_path=mqtt_broker_stdout_path,
            stderr_path=mqtt_broker_stderr_path,
        )

    def stop(self):
        if self.process is None:
            return

        self.process.terminate()
        try:
            self.process.wait(5)
        except TimeoutExpired:
            self.process.kill()

        self.process = None
