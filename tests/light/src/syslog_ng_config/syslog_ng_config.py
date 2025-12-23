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
import typing

from src.common.file import File
from src.common.operations import cast_to_list
from src.syslog_ng_config import stringify
from src.syslog_ng_config.renderer import ConfigRenderer
from src.syslog_ng_config.statement_group import StatementGroup
from src.syslog_ng_config.statements import ArrowedOptions
from src.syslog_ng_config.statements.destinations.example_destination import ExampleDestination
from src.syslog_ng_config.statements.destinations.file_destination import FileDestination
from src.syslog_ng_config.statements.destinations.network_destination import NetworkDestination
from src.syslog_ng_config.statements.destinations.snmp_destination import SnmpDestination
from src.syslog_ng_config.statements.destinations.unix_dgram_destination import UnixDgramDestination
from src.syslog_ng_config.statements.destinations.unix_stream_destination import UnixStreamDestination
from src.syslog_ng_config.statements.filters.filter import Filter
from src.syslog_ng_config.statements.filters.filter import Match
from src.syslog_ng_config.statements.filters.filter import RateLimit
from src.syslog_ng_config.statements.logpath.logpath import LogPath
from src.syslog_ng_config.statements.parsers.db_parser import DBParser
from src.syslog_ng_config.statements.parsers.parser import Parser
from src.syslog_ng_config.statements.rewrite.rewrite import CreditCardHash
from src.syslog_ng_config.statements.rewrite.rewrite import CreditCardMask
from src.syslog_ng_config.statements.rewrite.rewrite import Set
from src.syslog_ng_config.statements.rewrite.rewrite import SetPri
from src.syslog_ng_config.statements.rewrite.rewrite import SetTag
from src.syslog_ng_config.statements.sources.example_msg_generator_source import ExampleMsgGeneratorSource
from src.syslog_ng_config.statements.sources.file_source import FileSource
from src.syslog_ng_config.statements.sources.internal_source import InternalSource
from src.syslog_ng_config.statements.sources.network_source import NetworkSource
from src.syslog_ng_config.statements.sources.syslog_source import SyslogSource
from src.syslog_ng_config.statements.template.template import Template
from src.syslog_ng_config.statements.template.template import TemplateFunction
from src.syslog_ng_ctl.legacy_stats_handler import LegacyStatsHandler
from src.syslog_ng_ctl.prometheus_stats_handler import MetricFilter
from src.syslog_ng_ctl.prometheus_stats_handler import PrometheusStatsHandler
from src.syslog_ng_ctl.prometheus_stats_handler import Sample


logger = logging.getLogger(__name__)


class SyslogNgConfig(object):
    def __init__(
        self,
        version: str,
        stats_handler: LegacyStatsHandler,
        prometheus_stats_handler: PrometheusStatsHandler,
        teardown,
    ) -> None:
        self.__raw_config = None
        self.__syslog_ng_config = {
            "version": version,
            "includes": [],
            "global_options": {},
            "preamble": "",
            "templates": [],
            "statement_groups": [],
            "logpath_groups": [],
        }
        self.__stats_handler = stats_handler
        self.__prometheus_stats_handler = prometheus_stats_handler
        self.teardown = teardown

    stringify = staticmethod(stringify)

    @property
    def stats_handler(self) -> LegacyStatsHandler:
        return self.__stats_handler

    @property
    def prometheus_stats_handler(self) -> PrometheusStatsHandler:
        return self.__prometheus_stats_handler

    def arrowed_options(self, *args, **kwargs):
        return ArrowedOptions(*args, **kwargs)

    def set_raw_config(self, raw_config):
        self.__raw_config = raw_config

    def write_config(self, config_path):
        if self.__raw_config:
            rendered_config = self.__raw_config
        else:
            rendered_config = ConfigRenderer(self.__syslog_ng_config).get_rendered_config()
        logger.info("Generated syslog-ng config\n{}\n".format(rendered_config))

        syslog_ng_config_file = File(config_path)
        syslog_ng_config_file.write_content_and_close(rendered_config)

    def set_version(self, version):
        self.__syslog_ng_config["version"] = version

    def get_version(self):
        return self.__syslog_ng_config["version"]

    def add_include(self, include):
        self.__syslog_ng_config["includes"].append(include)

    def update_global_options(self, **options):
        self.__syslog_ng_config["global_options"].update(options)

    def add_preamble(self, preamble):
        self.__syslog_ng_config["preamble"] += "\n" + preamble

    def add_template(self, template):
        self.__syslog_ng_config["templates"].append(template)

    def create_file_source(self, **options):
        file_source = FileSource(**options)
        self.teardown.register(file_source.close_file)
        return file_source

    def create_example_msg_generator_source(self, **options):
        return ExampleMsgGeneratorSource(**options)

    def create_internal_source(self, **options):
        return InternalSource(**options)

    def create_network_source(self, **options):
        return NetworkSource(**options)

    def create_syslog_source(self, **options):
        return SyslogSource(**options)

    def create_rewrite_set(self, template, **options):
        return Set(template, **options)

    def create_rewrite_set_tag(self, tag, **options):
        return SetTag(tag, **options)

    def create_rewrite_set_pri(self, pri, **options):
        return SetPri(pri, **options)

    def create_template(self, template, **options):
        return Template(template, **options)

    def create_template_function(self, template, **options):
        return TemplateFunction(template, **options)

    def create_filter(self, expr=None, **options):
        return Filter("", self.__stats_handler, self.__prometheus_stats_handler, [expr] if expr else [], **options)

    def create_rate_limit_filter(self, **options):
        return RateLimit(self.__stats_handler, self.__prometheus_stats_handler, **options)

    def create_match_filter(self, **options):
        return Match(self.__stats_handler, self.__prometheus_stats_handler, **options)

    def create_app_parser(self, **options):
        return Parser("app-parser", self.__stats_handler, self.__prometheus_stats_handler, **options)

    def create_checkpoint_parser(self, **options):
        return Parser("checkpoint-parser", self.__stats_handler, self.__prometheus_stats_handler, **options)

    def create_panos_parser(self, **options):
        return Parser("panos-parser", self.__stats_handler, self.__prometheus_stats_handler, **options)

    def create_regexp_parser(self, **options):
        return Parser("regexp-parser", self.__stats_handler, self.__prometheus_stats_handler, **options)

    def create_csv_parser(self, **options):
        return Parser("csv-parser", self.__stats_handler, self.__prometheus_stats_handler, **options)

    def create_metrics_probe(self, **options):
        return Parser("metrics_probe", self.__stats_handler, self.__prometheus_stats_handler, **options)

    def create_syslog_parser(self, **options):
        return Parser("syslog-parser", self.__stats_handler, self.__prometheus_stats_handler, **options)

    def create_sdata_parser(self, **options):
        return Parser("sdata-parser", self.__stats_handler, self.__prometheus_stats_handler, **options)

    def create_group_lines_parser(self, **options):
        return Parser("group-lines", self.__stats_handler, self.__prometheus_stats_handler, **options)

    def create_cisco_parser(self, **options):
        return Parser("cisco-parser", self.__stats_handler, self.__prometheus_stats_handler, **options)

    def create_mariadb_audit_parser(self, **options):
        return Parser("mariadb-audit-parser", self.__stats_handler, self.__prometheus_stats_handler, **options)

    def create_postgresql_csvlog_parser(self, **options):
        return Parser("postgresql-csvlog-parser", self.__stats_handler, self.__prometheus_stats_handler, **options)

    def create_file_destination(self, **options):
        file_destination = FileDestination(self.__stats_handler, self.__prometheus_stats_handler, **options)
        self.teardown.register(file_destination.close_file)
        return file_destination

    def create_example_destination(self, **options):
        example_destination = ExampleDestination(self.__stats_handler, self.__prometheus_stats_handler, **options)
        self.teardown.register(example_destination.close_file)
        return example_destination

    def create_snmp_destination(self, **options):
        return SnmpDestination(self.__stats_handler, self.__prometheus_stats_handler, **options)

    def create_network_destination(self, **options):
        network_destination = NetworkDestination(self.__stats_handler, self.__prometheus_stats_handler, **options)
        self.teardown.register(network_destination.stop_listener)
        return network_destination

    def create_unix_dgram_destination(self, **options):
        unix_dgram_destination = UnixDgramDestination(self.__stats_handler, self.__prometheus_stats_handler, **options)
        self.teardown.register(unix_dgram_destination.stop_listener)
        return unix_dgram_destination

    def create_unix_stream_destination(self, **options):
        unix_stream_source = UnixStreamDestination(self.__stats_handler, self.__prometheus_stats_handler, **options)
        self.teardown.register(unix_stream_source.stop_listener)
        return unix_stream_source

    def create_db_parser(self, config, **options):
        return DBParser(self.__stats_handler, self.__prometheus_stats_handler, config, **options)

    def create_rewrite_credit_card_mask(self, **options):
        return CreditCardMask(**options)

    def create_rewrite_credit_card_hash(self, **options):
        return CreditCardHash(**options)

    def create_logpath(self, name=None, statements=None, flags=None):
        logpath = self.__create_logpath_with_conversion(name, statements, flags)
        self.__syslog_ng_config["logpath_groups"].append(logpath)
        return logpath

    def create_inner_logpath(self, name=None, statements=None, flags=None):
        inner_logpath = self.__create_logpath_with_conversion(name, statements, flags)
        return inner_logpath

    def create_statement_group(self, statements):
        statement_group = StatementGroup(statements)
        self.__syslog_ng_config["statement_groups"].append(statement_group)
        return statement_group

    def __create_statement_group_if_needed(self, item):
        if isinstance(item, (StatementGroup, LogPath)):
            return item
        else:
            return self.create_statement_group(item)

    def __create_logpath_with_conversion(self, name, items, flags):
        return self.__create_logpath_group(
            name,
            map(self.__create_statement_group_if_needed, cast_to_list(items)),
            flags,
        )

    def __create_logpath_group(self, name=None, statements=None, flags=None):
        logpath = LogPath(self.__prometheus_stats_handler, name)
        if statements:
            logpath.add_groups(statements)
        if flags:
            logpath.add_flags(cast_to_list(flags))
        return logpath

    def get_prometheus_samples(self, metric_filter: typing.List[MetricFilter]) -> typing.List[Sample]:
        return self.prometheus_stats_handler.get_samples(metric_filter)
