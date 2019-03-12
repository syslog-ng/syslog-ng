#!/usr/bin/env python
#############################################################################
# Copyright (c) 2015-2018 Balabit
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
from pathlib2 import PosixPath
from src.syslog_ng.syslog_ng_paths import SyslogNgPaths


def test_syslog_ng_paths(fake_testcase_parameters):
    syslog_ng_paths = SyslogNgPaths(fake_testcase_parameters)
    syslog_ng_paths.set_syslog_ng_paths(instance_name="server")
    assert set(list(syslog_ng_paths._SyslogNgPaths__syslog_ng_paths)) == {"dirs", "file_paths", "binary_file_paths"}
    assert set(list(syslog_ng_paths._SyslogNgPaths__syslog_ng_paths["dirs"])) == {"working_dir", "install_dir"}
    assert set(list(syslog_ng_paths._SyslogNgPaths__syslog_ng_paths["file_paths"])) == {
        "config_path",
        "persist_path",
        "pid_path",
        "control_socket_path",
        "stderr",
        "stdout",
        "valgrind",
    }
    assert set(list(syslog_ng_paths._SyslogNgPaths__syslog_ng_paths["binary_file_paths"])) == {
        "syslog_ng_binary",
        "syslog_ng_ctl",
    }


def test_syslog_ng_paths_parent_class_of_paths(fake_testcase_parameters):
    syslog_ng_paths = SyslogNgPaths(fake_testcase_parameters)
    syslog_ng_paths.set_syslog_ng_paths(instance_name="server")
    for __key, value in syslog_ng_paths._SyslogNgPaths__syslog_ng_paths["file_paths"].items():
        assert isinstance(value, PosixPath) is True

    for __key, value in syslog_ng_paths._SyslogNgPaths__syslog_ng_paths["dirs"].items():
        assert isinstance(value, PosixPath) is True

    for __key, value in syslog_ng_paths._SyslogNgPaths__syslog_ng_paths["binary_file_paths"].items():
        assert isinstance(value, PosixPath) is True


def test_syslog_ng_paths_client_relay_server(fake_testcase_parameters):
    syslog_ng_paths_server = SyslogNgPaths(
        fake_testcase_parameters
    ).set_syslog_ng_paths(instance_name="server")
    syslog_ng_paths_relay = SyslogNgPaths(
        fake_testcase_parameters
    ).set_syslog_ng_paths(instance_name="relay")
    syslog_ng_paths_client = SyslogNgPaths(
        fake_testcase_parameters
    ).set_syslog_ng_paths(instance_name="client")

    assert syslog_ng_paths_client.get_instance_name() == "client"
    assert syslog_ng_paths_relay.get_instance_name() == "relay"
    assert syslog_ng_paths_server.get_instance_name() == "server"


def test_instance_already_configured(fake_testcase_parameters):
    syslog_ng_paths_server = SyslogNgPaths(
        fake_testcase_parameters
    ).set_syslog_ng_paths(instance_name="server")
    with pytest.raises(Exception):
        syslog_ng_paths_server.set_syslog_ng_paths(instance_name="client")
