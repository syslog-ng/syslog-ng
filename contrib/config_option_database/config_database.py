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
import json

from utils.ConfigOptions import get_driver_options
from argparse import ArgumentParser


def parse_args():
    parser = ArgumentParser()
    parser.add_argument('context', type=str, help='source/destination')
    parser.add_argument('driver', type=str, help='driver')
    parser.add_argument('--rebuild', '-r', action='store_true')
    args = parser.parse_args()
    return (args.context, args.driver, args.rebuild)


def build_db():
    db = {'source': {}, 'destination': {}}
    for context, driver, keyword, arguments, parents in get_driver_options():
        db[context].setdefault(driver, {'options': [], 'blocks': {}})
        add_to = db[context][driver]
        for parent in parents:
            add_to['blocks'].setdefault(parent, {'options': [], 'blocks': {}})
            add_to = add_to['blocks'][parent]
        add_to['options'].append((keyword, arguments))
    return db


def get_db(rebuild):
    cache_file = Path(__file__).parents[0] / '.cache' / 'options.json'
    Path.mkdir(cache_file.parents[0], exist_ok=True)
    if rebuild or not cache_file.exists():
        db = build_db()
        with cache_file.open('w') as f:
            json.dump(db, f, indent=2)
    else:
        with cache_file.open() as f:
            db = json.load(f)
    return db


def print_options_helper(block, depth):
    indent = '  ' * depth
    for keyword, arguments in sorted(block['options']):
        if keyword:
            print(indent + '{}({})'.format(keyword, ' '.join(arguments)))
        else:
            print(indent + '{}'.format(' '.join(arguments)))
    for key in sorted(block['blocks'].keys()):
        print(indent + '{}('.format(key))
        print_options_helper(block['blocks'][key], depth + 1)
        print(indent + ')')


def print_options(db, context, driver):
    print('{} {}('.format(context, driver))
    print_options_helper(db[context][driver], 1)
    print(')')


def main():
    context, driver, rebuild = parse_args()
    db = get_db(rebuild)
    print_options(db, context, driver)


if __name__ == '__main__':
    main()
