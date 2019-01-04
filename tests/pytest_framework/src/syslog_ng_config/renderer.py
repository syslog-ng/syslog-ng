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

from pathlib2 import Path


class ConfigRenderer(object):
    def __init__(self, syslog_ng_config, instance_paths):
        self.__syslog_ng_config = syslog_ng_config
        self.__instance_paths = instance_paths
        self.__syslog_ng_config_content = ""
        self.__render()

    def get_rendered_config(self):
        return self.__syslog_ng_config_content

    def __render(self, re_create_config=None):
        if re_create_config:
            self.__syslog_ng_config_content = ""
        if self.__syslog_ng_config["version"]:
            self.__render_version()
        if self.__syslog_ng_config["statement_groups"]:
            self.__render_statement_groups()
        if self.__syslog_ng_config["logpath_groups"]:
            self.__render_logpath_groups(self.__syslog_ng_config["logpath_groups"])

    def __render_version(self):
        self.__syslog_ng_config_content += "@version: {}\n".format(self.__syslog_ng_config["version"])

    def __render_positional_options(self, driver_options, positional_options):
        for option_name, option_value in driver_options.items():
            if option_name in positional_options:
                if str(self.__instance_paths.get_working_dir()) not in str(option_value):
                    driver_options[option_name] = Path(self.__instance_paths.get_working_dir(), option_value)
                option_value = str(driver_options[option_name])
                self.__syslog_ng_config_content += "        {}\n".format(option_value)

    def __render_driver_options(self, driver_options, positional_options):
        for option_name, option_value in driver_options.items():
            if option_name not in positional_options:
                self.__syslog_ng_config_content += "        {}({})\n".format(option_name, option_value)

    def __render_statement_groups(self):
        for statement_group in self.__syslog_ng_config["statement_groups"]:
            # statement header
            self.__syslog_ng_config_content += "\n{} {} {{\n".format(
                statement_group.group_type, statement_group.group_id
            )

            for statement in statement_group.statements:
                # driver header
                self.__syslog_ng_config_content += "    {} (\n".format(statement.driver_name)

                # driver options
                self.__render_positional_options(statement.options, statement.positional_option_name)
                self.__render_driver_options(statement.options, statement.positional_option_name)

                # driver footer
                self.__syslog_ng_config_content += "    );\n"

            # statement footer
            self.__syslog_ng_config_content += "};\n"

    def __render_logpath_groups(self, logpath_groups):
        for logpath_group in logpath_groups:
            self.__syslog_ng_config_content += "\nlog {\n"
            for statement_group in logpath_group.logpath:
                self.__syslog_ng_config_content += "    {}({});\n".format(
                    statement_group.group_type, statement_group.group_id
                )
            self.__syslog_ng_config_content += "};\n"
