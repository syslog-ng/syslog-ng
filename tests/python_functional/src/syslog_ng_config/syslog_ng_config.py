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
logger = logging.getLogger(__name__)
from src.driver_io.file.file_io import FileIO
from src.syslog_ng_config.renderer import ConfigRenderer
from src.syslog_ng_config.statements.logpath.logpath import LogPath
from src.syslog_ng_config.statements.sources.file_source import FileSource
from src.syslog_ng_config.statements.sources.source_driver import SourceDriver
from src.syslog_ng_config.statements.destinations.file_destination import FileDestination
from src.syslog_ng_config.statement_group import StatementGroup
from src.common.operations import cast_to_list
from src.syslog_ng_config.statements.filters.filter import Filter


class SyslogNgConfig(object):
    def __init__(self, working_dir, version):
        self.__working_dir = working_dir
        self.__logger = logging.getLogger("SyslogNgConfig")
        self.__raw_config = None
        self.__syslog_ng_config = {
            "version": version,
            "global_options": {},
            "statement_groups": [],
            "logpath_groups": [],
        }

    def set_version(self, version):
        self.__syslog_ng_config["version"]=version

    def set_raw_config(self, raw_config):
        self.__raw_config = raw_config

    def write_content(self, config_path):
        if self.__raw_config:
            rendered_config = self.__raw_config
        else:
            rendered_config = ConfigRenderer(self.__syslog_ng_config, self.__working_dir).get_rendered_config()
        logger.info(
            "Used config \
        \n->Content:[{}]".format(
                rendered_config
            )
        )
        FileIO(config_path).rewrite(rendered_config)

    def create_statement_group_if_needed(self, item):
        if isinstance(item, (StatementGroup, LogPath)):
            return item
        else:
            return self.create_statement_group(item)

    def __create_logpath_with_conversion(self, items, flags):
        return self.__create_logpath_group(
            map(self.create_statement_group_if_needed, cast_to_list(items)),
            flags)

    @staticmethod
    def __create_logpath_group(statements=None, flags=None):
        logpath = LogPath()
        if statements:
            logpath.add_groups(statements)
        if flags:
            logpath.add_flags(cast_to_list(flags))
        return logpath

    def create_global_options(self, **kwargs):
        self.__syslog_ng_config["global_options"].update(kwargs)

    def create_file_source(self, **kwargs):
        return FileSource(self.__working_dir, **kwargs)

    def create_file_destination(self, **kwargs):
        return FileDestination(self.__working_dir, **kwargs)

    def create_filter(self, **kwargs):
        return Filter(**kwargs)

    def create_statement_group(self, statements):
        statement_group = StatementGroup(statements)
        self.__syslog_ng_config["statement_groups"].append(statement_group)
        return statement_group

    def create_logpath(self, statements=None, flags=None):
        logpath = self.__create_logpath_with_conversion(statements, flags)
        self.__syslog_ng_config["logpath_groups"].append(logpath)
        return logpath

    def create_inner_logpath(self, statements=None, flags=None):
        inner_logpath = self.__create_logpath_with_conversion(statements, flags)
        return inner_logpath

    def create_example_msg_generator(self, **options):
        generator_source = SourceDriver(None)
        generator_source.driver_name = "example_msg_generator"
        generator_source.DEFAULT_MESSAGE = "-- Generated message. --"
        generator_source.options = options
        return generator_source
