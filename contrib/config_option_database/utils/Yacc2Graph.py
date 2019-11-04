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

from subprocess import DEVNULL, Popen
from tempfile import NamedTemporaryFile


def _run_in_shell(command):
    proc = Popen(command, stderr=DEVNULL, stdout=DEVNULL)
    proc.wait()
    return proc.returncode == 0


def _write_to_file(content):
    file = NamedTemporaryFile(mode='w')
    file.write(content)
    file.flush()
    file.seek(0)
    return file


def _yacc2xml(yacc_content):
    with _write_to_file(yacc_content) as file:
        xml_filepath = NamedTemporaryFile().name
        try:
            if not _run_in_shell(['bison', '--xml=' + xml_filepath, '--output=/dev/null', file.name]):
                raise Exception('Failed to convert to xml:\n{}\n'.format(yacc_content))
        except FileNotFoundError:
            raise Exception('bison executable not found')
        return xml_filepath
