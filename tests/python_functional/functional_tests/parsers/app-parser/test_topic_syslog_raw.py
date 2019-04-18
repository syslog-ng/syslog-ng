#!/usr/bin/env python
#############################################################################
# Copyright (c) 2019 Balabit
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
import pytest

test_parameters_raw = [
    ("<189>29: foo: *Apr 29 13:58:40.411: %SYS-5-CONFIG_I: Configured from console by console", "${.app.name}", "cisco"),
    ("<190>30: foo: *Apr 29 13:58:46.411: %SYS-6-LOGGINGHOST_STARTSTOP: Logging to host 192.168.1.239 stopped - CLI initiated", "${.app.name}", "cisco"),
    ("<190>31: foo: *Apr 29 13:58:46.411: %SYS-6-LOGGINGHOST_STARTSTOP: Logging to host 192.168.1.239 started - CLI initiated<189>32: 0.0.0.0: *Apr 29 13:59:12.491: %SYS-5-CONFIG_I: Configured from console by console<189>33: 0.0.0.0: *Apr 29 13:59:26.415: %SYS-5-CONFIG_I: Configured from console by console<189>34: 0.0.0.0: *Apr 29 13:59:56.603: %SYS-5-CONFIG_I: Configured from console by console^[[<189>35: *Apr 29 14:00:16.059: %SYS-5-CONFIG_I: Configured from console by console", "${.app.name}", "cisco"),
    ("<190>32: foo: *Apr 29 13:58:46.411: %SYSMGR-STANDBY-3-SHUTDOWN_START: The System Manager has started the shutdown procedure.", "${.app.name}", "cisco"),
    (r"""<134>{\"count\": 1, \"supporting_data\": {\"data_values\": [\"x.x.x.x\", \"user@domain.com\"], \"data_type\": \"user\"}, \"organization_unit\": \"domain/domain/Domain Users/Enterprise Users\", \"severity_level\": 2, \"category\": null, \"timestamp\": 1547421943, \"_insertion_epoch_timestamp\": 1547421943, \"ccl\": \"unknown\", \"user\": \"user@domain.com\", \"audit_log_event\": \"Login Successful\", \"ur_normalized\": \"user@domain.com\", \"_id\": \"936289\", \"type\": \"admin_audit_logs\", \"appcategory\": null}""", "${.app.name}", "netskope"),
]

@pytest.mark.parametrize("input_message, template, expected_value", test_parameters_raw,
                         ids=list(map(str, range(len(test_parameters_raw)))))
def test_application_raw(config, syslog_ng, input_message, template, expected_value):
    config.add_include("scl.conf")

    generator_source = config.create_example_msg_generator(num=1, template="\"" + input_message + "\"")
    app_parser = config.create_app_parser(topic="syslog-raw")
    file_destination = config.create_file_destination(file_name="output.log", template="\"" + template + "\\n" + "\"")
    config.create_logpath(statements=[generator_source, app_parser, file_destination])

    syslog_ng.start(config)
    assert file_destination.read_log() == expected_value + "\n"
