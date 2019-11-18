#!/usr/bin/env python3
#############################################################################
# Copyright (c) 2019 Balabit
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

from pathlib import Path
from sys import argv


def get_grammar_files():
    root_dir = Path(__file__).resolve().parents[3]
    exclude = ['rewrite-expr-grammar.ym', 'native-grammar.ym']
    files = []
    files.extend(list((root_dir / 'modules').rglob('*.ym')))
    files.extend(list((root_dir / 'lib').rglob('*.ym')))
    files.append(root_dir / 'lib' / 'cfg-grammar.y')
    return list(filter(lambda f: f.name not in exclude, files))


def merge_grammars(output_filepath):
    files = get_grammar_files()
    declarations = set()
    blocks = r'%%' + '\n'

    for filepath in files[1:]:
        with filepath.open() as f:
            in_block = False
            for line in f:
                if line.startswith('%token') or line.startswith(r'%left') or line.startswith('%type'):
                    declarations.add(line)
                elif line.startswith(r'%%'):
                    in_block = not in_block
                elif in_block:
                    blocks += line
    blocks += r'%%' + '\n'

    with open(output_filepath, 'w') as f:
        f.write(''.join(declarations) + blocks)


def main():
    output_filepath = 'syslog-grammar-merged.y'
    if len(argv) >= 2:
        output_filepath = argv[1]
    merge_grammars(output_filepath)


if __name__ == '__main__':
    main()
