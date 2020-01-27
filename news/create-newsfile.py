#!/usr/bin/env python3
#############################################################################
# Copyright (c) 2020 Balabit
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

import re
import sys
from argparse import ArgumentParser
from pathlib import Path
from subprocess import PIPE, Popen

news_dir = Path(__file__).resolve().parent
root_dir = news_dir.parent
newsfile = root_dir / 'NEWS.md'


def print_usage_if_needed():
    ArgumentParser(usage="\rCreates NEWS.md file from the entries in the news/ folder.\n"
                         "It also deletes the entry files.").parse_args()


def create_block(block_name, files):
    block = '## {}\n\n'.format(block_name)
    for f in files:
        entry = ''
        try:
            pr_id = re.findall(r'\d+.md$', f.name)[0][:-3]
        except IndexError:
            sys.exit('Invalid filename: {}'.format(f.name))
        entry += ' * {} (#{})'.format(f.read_text().rstrip(), pr_id)
        entry = entry.replace('\n', '\n   ')
        block += entry + '\n'
    block += '\n'
    return block


def get_news_version():
    proc = Popen(r'git show HEAD:NEWS.md'.split(), cwd=str(root_dir), stdout=PIPE)
    stdout, _ = proc.communicate()
    stdout = stdout.decode()
    return stdout[:stdout.index('\n')]


def create_version():
    version = (root_dir / 'VERSION').read_text().rstrip()
    news_version = get_news_version()
    if version == news_version:
        print('VERSION file contains the same version as the current NEWS.md file.\n' \
              'Probably you are trying to create the newsfile before bumping the `VERSION` file.\n' \
              'Please provide version to be released.')
        version = input('Version to be released: ')
    return '{}\n{}\n\n'.format(version, len(version) * '=')


def create_highlights_block():
    return '## Highlights\n' \
           '\n' \
           '<Fill this block manually from the blocks below>\n' \
           '\n'


def create_standard_blocks():
    standard_blocks = ''
    blocks = [
        ('Features', 'feature-*.md'),
        ('Bugfixes', 'bugfix-*.md'),
        ('Notes to developers', 'developer-note-*.md'),
        ('Other changes', 'other-*.md'),
    ]
    for block_name, glob in blocks:
        entries = list(news_dir.glob(glob))
        if len(entries) > 0:
            standard_blocks += create_block(block_name, entries)
    return standard_blocks


def create_credits_block():
    return '## Credits\n' \
           '\n' \
           'syslog-ng is developed as a community project, and as such it relies\n' \
           'on volunteers, to do the work necessarily to produce syslog-ng.\n' \
           '\n' \
           'Reporting bugs, testing changes, writing code or simply providing\n' \
           'feedback are all important contributions, so please if you are a user\n' \
           'of syslog-ng, contribute.\n' \
           '\n' \
           'We would like to thank the following people for their contribution:\n' \
           '\n' \
           '<Fill this by the internal news file creating tool>\n'


def create_newsfile(news):
    newsfile.write_text(news)
    print('Newsfile created at {}\n'.format(newsfile.resolve()))


def cleanup():
    print("Cleaning up entry files with `git rm news/*-*.md`:")
    Popen("git rm news/*-*.md".split(), cwd=str(root_dir))


def create_news_content():
    news = create_version()
    news += create_highlights_block()
    news += create_standard_blocks()
    news += create_credits_block()
    return news


def main():
    print_usage_if_needed()
    news = create_news_content()
    create_newsfile(news)
    cleanup()


if __name__ == '__main__':
    main()
