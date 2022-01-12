#!/usr/bin/env python
#############################################################################
# Copyright (c) 2020 One Identity
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
from src.helpers.secure_logging.conftest import *  # noqa:F403, F401

seqnum = "$(iterate $(+ 1 $_) 0)"
message_base = "example-message"
example_message = "{}: {}".format(message_base, seqnum)
num_of_messages = 3


def test_secure_logging(config, syslog_ng, slog):
    output_file_name = "output.log"
    generator_source = config.create_example_msg_generator_source(num=num_of_messages, template=config.stringify(example_message), freq="0")
    secure_log_template = "$(slog -k {} -m {} $MSG)".format(slog.derived_key, slog.cmac)
    file_destination = config.create_file_destination(file_name=output_file_name, template=config.stringify(secure_log_template + '\n'))

    config.create_logpath(statements=[generator_source, file_destination])
    syslog_ng.start(config)

    logs = file_destination.read_logs(num_of_messages)
    # test for no clear text
    assert not any(map(lambda x: message_base in x, logs))

    decrypted = slog.decrypt(output_file_name)
    assert decrypted == ["{}: {}: {}".format(str(i).zfill(16), message_base, i) for i in range(num_of_messages)]
