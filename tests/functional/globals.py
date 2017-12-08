#############################################################################
# Copyright (c) 2007-2016 Balabit
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

def is_running_in_build_tree():
    return 'SYSLOG_NG_BINARY' not in os.environ

def get_module_path_from_binary():
    module_path = os.popen("%s --version | grep \"Module-Path:\" | cut -d ' ' -f 2" % get_syslog_ng_binary(), 'r').read().strip()
    return module_path

def format_module_path_for_intree_modules():
    module_path = ''
    for (root, dirs, files) in os.walk(os.path.abspath(os.path.join(os.environ['top_builddir'], 'modules'))):
        module_path += ':'.join(map(lambda x: root + '/' + x + '/.libs', dirs))
        module_path += ':'.join(map(lambda x: root + '/' + x, dirs))
        break
    return module_path

def get_module_path():
    if is_running_in_build_tree():
        module_path = format_module_path_for_intree_modules()
    else:
        module_path = get_module_path_from_binary()
    return module_path

def get_syslog_ng_binary():
    return os.getenv('SYSLOG_NG_BINARY', '../../syslog-ng/syslog-ng')

def is_premium():
    version = os.popen('%s -V' % get_syslog_ng_binary(), 'r').read()
    if version.find('premium-edition') != -1:
        return True
    return False

def has_module(module):
    avail_mods = os.popen('%s -V | grep ^Available-Modules: ' % get_syslog_ng_binary(), 'r').read()
    if avail_mods.find(module) != -1:
        return True
    return False


is_premium_edition = is_premium()
if is_premium_edition:
    logstore_store_supported = True
    wildcard_file_source_supported = True
else:
    logstore_store_supported = False
    wildcard_file_source_supported = True

port_number = os.getpid() % 30000 + 33000
ssl_port_number = port_number + 1
port_number_syslog = port_number + 2
port_number_network = port_number + 3

current_dir = os.getcwd()
try:
    src_dir = os.environ["srcdir"]
except KeyError:
    src_dir = current_dir
