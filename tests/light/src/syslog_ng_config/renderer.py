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
from src.syslog_ng_config import stringify
from src.syslog_ng_config.statements import ArrowedOptions
from src.syslog_ng_config.statements.filters.filter import Filter
from src.syslog_ng_config.statements.template.template import Template
from src.syslog_ng_config.statements.template.template import TemplateFunction


def render_version(version):
    return "@version: {}\n".format(version)


def render_includes(includes):
    include_lines = ['@include "{}"'.format(include) for include in includes]
    return "\n".join(include_lines)


def render_global_options(global_options):
    globals_options_header = "options {\n"
    globals_options_footer = "};\n"

    config_snippet = globals_options_header
    for option_name, option_value in global_options.items():
        config_snippet += "    {}({});\n".format(option_name, option_value)
    config_snippet += globals_options_footer

    return config_snippet


def render_positional_options(positional_parameters):
    config_snippet = ""
    for parameter in positional_parameters:
        config_snippet += "        {}\n".format(str(parameter))
    return config_snippet


def render_options(name, options):

    config_snippet = ""
    config_snippet += "        {}(\n".format(name)
    config_snippet += render_driver_options(options)
    config_snippet += "        )\n"

    return config_snippet


def render_arrowed_options(name, keys):
    config_snippet = "         {}(\n".format(name)
    for key, value in keys.items():
        config_snippet += "        {} => {}\n".format(stringify(key), value)
    config_snippet += "        )\n"

    return config_snippet


def render_template_statement(template):
    if not template.use_simple_statement:
        config_snippet = "template {} {{\n".format(template.name)
        config_snippet += "        template({});\n".format(stringify(template.template))
        if template.template_escape is not None:
            config_snippet += "        template-escape({});\n".format(template.template_escape)
        config_snippet += "};\n"
    else:
        config_snippet = "template {} {};\n".format(template.name, stringify(template.template))
    return config_snippet


def render_template_function(template):
    return "template-function {} {};\n".format(template.name, stringify(template.template))


def render_template(template):
    if isinstance(template, Template):
        return render_template_statement(template)
    elif isinstance(template, TemplateFunction):
        return render_template_function(template)


def render_templates(templates):
    config_snippet = ""
    for template in templates:
        config_snippet += render_template(template)
    return config_snippet


def render_list(name, options):
    config_snippet = ""

    for element in options:
        config_snippet += "        {}({})\n".format(name, element)

    return config_snippet


def render_name_value(name, value):
    return "        {}({})\n".format(name, value)


def render_driver(name, driver):
    return "        {}({})\n".format(name, render_statement(driver))


def render_driver_options(driver_options):
    config_snippet = ""

    for option_name, option_value in driver_options.items():
        if isinstance(option_value, ArrowedOptions):
            config_snippet += render_arrowed_options(option_name, option_value)
        elif isinstance(option_value, dict):
            config_snippet += render_options(option_name, option_value)
        elif (isinstance(option_value, tuple) or isinstance(option_value, list)):
            config_snippet += render_list(option_name, option_value)
        elif isinstance(option_value, Filter):
            config_snippet += render_driver(option_name, option_value)
        else:
            config_snippet += render_name_value(option_name, option_value)

    return config_snippet


def render_statement(statement):
    config_snippet = ""
    config_snippet += "    {} (\n".format(statement.driver_name)
    config_snippet += render_positional_options(statement.positional_parameters)
    config_snippet += render_driver_options(statement.options)
    config_snippet += "    )"

    return config_snippet


def render_statement_groups(statement_groups):
    config_snippet = ""

    for statement_group in statement_groups:
        # statement header
        config_snippet += "\n{} {} {{\n".format(
            statement_group.group_type, statement_group.group_id,
        )

        for statement in statement_group:
            # driver header
            config_snippet += render_statement(statement)

            # driver footer
            config_snippet += ";\n"

            # statement footer
        config_snippet += "};\n"

    return config_snippet


def _indent(string, depth):
    return depth * "    " + string


def render_logpath_groups(logpath_groups, depth=0):
    config_snippet = "\n"

    for logpath_group in logpath_groups:
        if logpath_group.name:
            config_snippet += _indent("log " + logpath_group.name + " {\n", depth)
        else:
            config_snippet += _indent("log {\n", depth)
        for statement_group in logpath_group.logpath:
            if statement_group.group_type == "log":
                config_snippet += render_logpath_groups(logpath_groups=[statement_group], depth=depth + 1)
            else:
                config_snippet += _indent("{}({});\n".format(statement_group.group_type, statement_group.group_id), depth + 1)
        if logpath_group.flags:
            config_snippet += _indent("flags({});\n".format("".join(logpath_group.flags)), depth + 1)
        config_snippet += _indent("};\n", depth)

    return config_snippet


class ConfigRenderer(object):
    def __init__(self, syslog_ng_config):
        self.__syslog_ng_config = syslog_ng_config
        self.__syslog_ng_config_content = ""
        self.__render()

    def get_rendered_config(self):
        return self.__syslog_ng_config_content

    def __render(self, re_create_config=None):

        version = self.__syslog_ng_config["version"]
        includes = self.__syslog_ng_config["includes"]
        global_options = self.__syslog_ng_config["global_options"]
        preamble = self.__syslog_ng_config["preamble"]
        templates = self.__syslog_ng_config["templates"]
        statement_groups = self.__syslog_ng_config["statement_groups"]
        logpath_groups = self.__syslog_ng_config["logpath_groups"]

        config = ""

        if version:
            config += render_version(version)
        if includes:
            config += render_includes(includes)
        if global_options:
            config += render_global_options(global_options)
        if preamble:
            config += preamble
        if templates:
            config += render_templates(templates)
        if statement_groups:
            config += render_statement_groups(statement_groups)
        if logpath_groups:
            config += render_logpath_groups(logpath_groups)

        if re_create_config:
            self.__syslog_ng_config_content = config
        else:
            self.__syslog_ng_config_content += config
