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
import os

import pytest

from src.helpers.secure_logging.conftest import *  # noqa:F403, F401

example_message = "example-message"
num_of_messages = 3


@pytest.mark.skipif("cmake_build" in os.environ, reason="only autotools is supported for now")
def test_secure_logging(config, syslog_ng, slog):
    output_file_name = "output.log"
    generator_source = config.create_example_msg_generator_source(num=num_of_messages, template=config.stringify(example_message), freq="0.001")
    secure_log_template = "$(slog -k {} -m {} $MSG)".format(slog.derived_key, slog.cmac)
    file_destination = config.create_file_destination(file_name=output_file_name, template=config.stringify(secure_log_template + '\n'))

    config.create_logpath(statements=[generator_source, file_destination])
    syslog_ng.start(config)

    logs = file_destination.read_logs(num_of_messages)
    assert not any(map(lambda x: example_message in x, logs))

    decrypted = slog.decrypt(output_file_name)
    assert all(map(lambda x: example_message in x, decrypted))
