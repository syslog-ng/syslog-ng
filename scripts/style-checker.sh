#!/bin/bash
#############################################################################
# Copyright (c) 2016 Balabit
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


action="$1"
root_dir="$2"

function setup_root_dir
{
    if [ "$root_dir" == "" ]; then
        root_dir="$(pwd)"
    fi
}

function astyle_c_check
{
    astyle --options="$root_dir/.astylerc" --dry-run "$root_dir/*.h" "$root_dir/*.c" | grep "Formatted" | tee badly-formatted-files.list | wc -l | while read badly_formatted_c_files
    do
        echo "Number of badly formatted files: $badly_formatted_c_files"
        if [ "$badly_formatted_c_files" == "0" ]; then
            exit 0
        else
            cat badly-formatted-files.list
            exit 1
        fi
    done
}

function astyle_c_format
{
    astyle --options="$root_dir/.astylerc" "$root_dir/*.h" "$root_dir/*.c" | grep "Formatted"
    exit 0
}

function print_help
{
    echo "Format C source files to comply with coding standards of syslog-ng"
    echo "Possible actions:"
    echo "    check    Check the format of sources in current directory"
    echo "    format   Format sources in current directory"
    echo "    help     Print this message"
}

function run_checker
{
    setup_root_dir
    if [ "$action" == "check" ]; then
        echo "Checking C source files"
        astyle_c_check
    elif [ "$action" == "format" ]; then
        echo "Formatting C source files"
        astyle_c_format
    elif [ "$action" == "help" ]; then
        print_help
    else
        echo "ERROR: Unknown command"
        print_help
        exit 2
    fi
}

run_checker
