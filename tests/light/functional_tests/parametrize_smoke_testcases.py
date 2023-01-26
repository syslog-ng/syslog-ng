#!/usr/bin/env python
#############################################################################
# Copyright (c) 2022 Andras Mitzki <mitzkia@gmail.com>
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
import sys

if "GITHUB_WORKSPACE" in os.environ:
    sys.path.append(os.path.join("/__w/syslog-ng/syslog-ng/contrib/config_option_database/"))
elif "IMAGE_PLATFORM" in os.environ:
    sys.path.append(os.path.join("/source/contrib/config_option_database/"))
else:
    sys.path.append(os.path.join("../../contrib/config_option_database/"))

from utils.ConfigOptions import get_driver_options  # noqa: E402


def generate_options_and_values_for_driver(expected_context, expected_driver):
    option_value_type_to_value_map = {
        "<float>": 12.34,
        "<nonnegative-integer>": 4567,
        "<positive-integer>": 6789,
        "<string-list>": "aaa bbb ccc",
        "<string>": "'almafa'",
        "<template-content>": "'$MSG\n'",
        "<yesno>": "yes",
        "persist-only": "persist-only",
        "<string> <arrow> <template-content>": '"HOST" => "host$(iterate $(+ 1 $_) 0)"',
    }

    option_name_to_value_map = {
        "setup": "'pwd'",
        "startup": "'pwd'",
    }

    options = []

    def generate_option_properties_for_driver(expected_context, expected_driver):
        for context, driver, option_names, option_types, block_names in get_driver_options():
            if context == expected_context and driver == expected_driver:
                yield option_names, option_types, block_names

    def get_option_value(option_name, option_type):
        if option_type == "":
            return ""
        if option_name in option_name_to_value_map:
            return option_name_to_value_map[option_name]
        else:
            return option_value_type_to_value_map[option_type]

    def build_option_block(block_names, option_and_value):
        option_block = {}

        def update_option_block(option_block, subkey):
            option_block.update({subkey: {}})
            return option_block[subkey]

        for index, block_name in enumerate(block_names, start=1):
            if option_block == {}:
                working_option_block = update_option_block(option_block, block_name)
            else:
                working_option_block = update_option_block(working_option_block, block_name)
            if index == len(block_names):
                working_option_block.update(option_and_value)
        return option_block

    for option_names, option_types, block_names in generate_option_properties_for_driver(expected_context, expected_driver):
        if len(option_types) == 1:
            option_type = option_types[0]
        else:
            option_type = " ".join(option_types)
        for option_name in option_names.split("/"):
            if not block_names:
                options.append({option_name: get_option_value(option_name, option_type)})
            else:
                result_option_block = build_option_block(block_names, {option_name: get_option_value(option_name, option_type)})
                options.append(result_option_block)

    return options


def generate_id_name(param):
    def stringify_parameter(param):
        return repr(param).replace("{", "").replace("}", "").replace("'", "").replace(":", "").replace('"', "").replace(" ", "_")
    return stringify_parameter(param)
