#!/usr/bin/env python
#############################################################################
# Copyright (c) 2023 Balazs Scheidler <bazsi77@gmail.com>
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


def test_group_lines_parser(config, syslog_ng):

    traceback = """Traceback (most recent call last):
File "./lib/merge-grammar.py", line 62, in <module>
  for line in fileinput.input(openhook=fileinput.hook_encoded("utf-8")):
File "/usr/lib/python3.8/fileinput.py", line 248, in __next__
  line = self._readline()
File "/usr/lib/python3.8/fileinput.py", line 368, in _readline
  return self._readline()
ValueError: This is the exception text at the end
"""

    config.update_global_options(stats_level=1)
    file_source = config.create_file_source(file_name="input.log")
    group_lines_parser = config.create_group_lines_parser(key=config.stringify("foo"), multi_line_mode="smart", timeout=10)

    file_destination = config.create_file_destination(file_name="output.log", template=config.stringify("$(format-welf MESSAGE)\n"))
    config.create_logpath(statements=[file_source, group_lines_parser, file_destination])

    syslog_ng.start(config)
    file_source.write_log('\n'.join(map(lambda line: 'python: ' + line, traceback.split('\n'))))
    file_source.write_log("whatvever: the exception text at the end\n")
    assert file_destination.read_log().strip() == 'MESSAGE="' + traceback.strip().replace('"', "\\\"").replace('\n', '\\n') + '"'
