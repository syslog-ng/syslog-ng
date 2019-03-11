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
import logging

from pathlib2 import Path

from src.common.operations import cast_to_list
from src.driver_io.file.file_io import FileIO
from src.message_reader.single_line_parser import SingleLineParser
from src.syslog_ng_config.renderer import ConfigRenderer
from src.syslog_ng_config.statement_group import StatementGroup
from src.syslog_ng_config.statements.destination_driver import DestinationDriver
from src.syslog_ng_config.statements.filter import Filter
from src.syslog_ng_config.statements.logpath import LogPath
from src.syslog_ng_config.statements.source_driver import SourceDriver


logger = logging.getLogger(__name__)


class SyslogNgConfig(object):
    def __init__(self, working_dir, version):
        self.__working_dir = working_dir
        self.__raw_config = None
        self.__syslog_ng_config = {
            "version": version,
            "global_options": {},
            "statement_groups": [],
            "logpath_groups": [],
        }
        self.driver_resources = {
            "file": {"driver_io": FileIO, "parser": SingleLineParser},
        }

    # Public API
    def set_raw_config(self, raw_config):
        self.__raw_config = raw_config

    def write_content(self, config_path):
        if self.__raw_config:
            rendered_config = self.__raw_config
        else:
            rendered_config = ConfigRenderer(self.__syslog_ng_config, self.__working_dir).get_rendered_config()
        logger.info("Generated syslog-ng config\n{}\n".format(rendered_config))
        FileIO(config_path).rewrite(rendered_config)

    def create_statement_group(self, statements):
        statement_group = StatementGroup(statements)
        self.__syslog_ng_config["statement_groups"].append(statement_group)
        return statement_group

    # Config elements
    def set_version(self, version):
        self.__syslog_ng_config["version"] = version

    def create_global_options(self, **kwargs):
        self.__syslog_ng_config["global_options"].update(kwargs)

    # Sources
    def create_file_source(self, **options):
        driver_name = "file"
        self.__set_file_name_option(options, "fsinput.log")
        file_source = SourceDriver(
            driver_name=driver_name,
            driver_resources=self.driver_resources[driver_name],
            options=options,
            positional_option="file_name",
            connection_options="file_name")
        return file_source

    @staticmethod
    def create_example_msg_generator(**options):
        driver_name = "example_msg_generator"
        if "num" not in options.keys():
            options['num'] = 1
        generator_source = SourceDriver(
            driver_name=driver_name,
            driver_resources=None,
            options=options,
            positional_option=None,
            connection_options=None)
        generator_source.DEFAULT_MESSAGE = "-- Generated message. --"
        return generator_source

    # Destinations
    def create_file_destination(self, **options):
        driver_name = "file"
        self.__set_file_name_option(options, "fdoutput.log")
        file_destination = DestinationDriver(
            driver_name=driver_name,
            driver_resources=self.driver_resources[driver_name],
            options=options,
            positional_option="file_name",
            connection_options="file_name")
        return file_destination

    # Filter
    def create_filter(self, **kwargs):
        return Filter(**kwargs)

    # Logpath
    def create_logpath(self, statements=None, flags=None):
        logpath = self.__create_logpath_with_conversion(statements, flags)
        self.__syslog_ng_config["logpath_groups"].append(logpath)
        return logpath

    def create_inner_logpath(self, statements=None, flags=None):
        inner_logpath = self.__create_logpath_with_conversion(statements, flags)
        return inner_logpath

    # Private API
    def __create_logpath_with_conversion(self, items, flags):
        return self.__create_logpath_group(
            map(self.__create_statement_group_if_needed, cast_to_list(items)),
            flags)

    @staticmethod
    def __create_logpath_group(statements=None, flags=None):
        logpath = LogPath()
        if statements:
            logpath.add_groups(statements)
        if flags:
            logpath.add_flags(cast_to_list(flags))
        return logpath

    def __create_statement_group_if_needed(self, item):
        if isinstance(item, (StatementGroup, LogPath)):
            return item
        else:
            return self.create_statement_group(item)

    def __set_file_name_option(self, options, default_file_name):
        if "file_name" in options.keys():
            # empty and custom
            options["file_name"] = Path(self.__working_dir, options["file_name"])
        else:
            # default
            options["file_name"] = Path(self.__working_dir, default_file_name)
