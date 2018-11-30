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
        if self.__syslog_ng_config["sources"]:
            self.__render_statements(root_statement="sources", statement_name="source")
        if self.__syslog_ng_config["filters"]:
            self.__render_statements(root_statement="filters", statement_name="filter")
        if self.__syslog_ng_config["destinations"]:
            self.__render_statements(root_statement="destinations", statement_name="destination")
        if self.__syslog_ng_config["logpaths"]:
            self.__render_logpath(self.__syslog_ng_config["logpaths"])

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

    def __render_statements(self, root_statement, statement_name):
        for statement_id, driver in self.__syslog_ng_config[root_statement].items():
            # statement header
            self.__syslog_ng_config_content += "\n{} {} {{\n".format(statement_name, statement_id)
            for dummy_driver_id, driver_properties in driver.items():
                driver_name = driver_properties["driver_name"]
                driver_options = driver_properties["driver_options"]
                # driver header
                self.__syslog_ng_config_content += "    {} (\n".format(driver_name)

                # driver options
                self.__render_positional_options(driver_options, driver_properties["positional_option"])
                self.__render_driver_options(driver_options, driver_properties["positional_option"])

                # driver footer
                self.__syslog_ng_config_content += "    );\n"

            # statement footer
            self.__syslog_ng_config_content += "};\n"

    def __render_logpath(self, logpath_node):
        for logpath_id in logpath_node:

            self.__syslog_ng_config_content += "\nlog {\n"
            for src_driver in logpath_node[logpath_id]["sources"]:
                self.__syslog_ng_config_content += "    source({});\n".format(src_driver)
            for filter in logpath_node[logpath_id]["filters"]:
                self.__syslog_ng_config_content += "    filter({});\n".format(filter)
            for dst_driver in logpath_node[logpath_id]["destinations"]:
                self.__syslog_ng_config_content += "    destination({});\n".format(dst_driver)

            if len(logpath_node[logpath_id]["logpaths"]) > 0:
                self.__render_logpath(logpath_node=logpath_node[logpath_id]["logpaths"])

            for flags in logpath_node[logpath_id]["flags"]:
                self.__syslog_ng_config_content += "    flags({});\n".format(flags)
            self.__syslog_ng_config_content += "};\n"
