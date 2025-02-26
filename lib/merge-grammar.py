#!/usr/bin/env python3
#############################################################################
# Copyright (c) 2024 Axoflow
# Copyright (c) 2024 Attila Szakacs <attila.szakacs@axoflow.com>
# Copyright (c) 2010-2017 Balabit
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
# As an additional exemption you are allowed to compile & link against the
# OpenSSL libraries as published by the OpenSSL project. See the file
# COPYING for details.
#
#############################################################################
#
# TODOs:
#  * be more clever about which rules to include as bison generates a lot of
#    warnings about unused rules
#

from __future__ import print_function
import fileinput
import os
import sys
import codecs

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
COMMON_GRAMMAR_FILE = 'lib/cfg-grammar.y'
BLOCKS = {}


def locate_file(file_name):
    full_path = os.path.join(os.environ.get('top_srcdir', ''), file_name)
    if not os.path.isfile(full_path):
        full_path = os.path.join(ROOT_DIR, file_name)
    if not os.path.isfile(full_path):
        sys.exit('Error opening ' + file_name)
    return full_path


def print_to_stdout(line):
    if sys.hexversion >= 0x3000000:
        sys.stdout.buffer.write(line.encode("utf-8"))
    else:
        print(line.encode("utf-8"), end='')


def collect_block_definitions(file_name):
    with codecs.open(locate_file(file_name), encoding="utf-8") as f:
        block_name = None
        block = []
        for line in f:
            if line.startswith('/* START_') and line.endswith(' */\n'):
                block_name = line[len('/* START_'):-len(' */\n')]
            elif block_name and line == '/* END_' + block_name + ' */\n':
                BLOCKS[block_name] = BLOCKS.get(block_name, '') + ''.join(block)
                block_name = None
                block = []
            elif block_name:
                block.append(line)


def include_block(block_type):
    try:
        print_to_stdout(BLOCKS[block_type])
    except KeyError:
        sys.exit('Block not found: ' + block_type)


def print_preprocessed_grammar():
    for line in fileinput.input(openhook=fileinput.hook_encoded("utf-8")):
        if line.startswith('/* INCLUDE_') and line.endswith(' */\n'):
            block_name = line[len('/* INCLUDE_'):-len(' */\n')]
            include_block(block_name)
        elif line.startswith('/* REQUIRE ') and line.endswith(' */\n'):
            file_name = line[len('/* REQUIRE '):-len(' */\n')]
            collect_block_definitions(file_name)
        else:
            print_to_stdout(line)


def main():
    collect_block_definitions(COMMON_GRAMMAR_FILE)
    print_preprocessed_grammar()


if __name__ == '__main__':
    main()
