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
# syslog-ng process control

import os, sys, signal, traceback, time, errno, re

from globals import *
from log import *
import messagegen

syslogng_pid = 0


def start_syslogng(conf, keep_persist=False, verbose=False):
    global syslogng_pid

    os.system('rm -rf test-*.log test-*.lgs test-*.db wildcard/* log-file')
    if not keep_persist:
        os.system('rm -f syslog-ng.persist')

    if not logstore_store_supported:
        conf = re.sub('logstore\(.*\);', '', conf)

    f = open('test.conf', 'w')
    f.write(conf)
    f.close()
# Check if verbose is true
    if verbose:
        verbose_opt = '-edv'
    else:
        verbose_opt = '-e'

    syslogng_pid = os.fork()
    if syslogng_pid == 0:
        os.putenv("RANDFILE", "rnd")
        module_path = get_module_path()
        rc = os.execl(get_syslog_ng_binary(), get_syslog_ng_binary(), '-f', 'test.conf', '--fd-limit', '1024', '-F', verbose_opt, '-p', 'syslog-ng.pid', '-R', 'syslog-ng.persist', '--no-caps', '--enable-core', '--seed', '--module-path', module_path)
        sys.exit(rc)
    time.sleep(5)
    print_user("Syslog-ng started")
    return True

def stop_syslogng():
    global syslogng_pid

    if syslogng_pid == 0:
        return True

    try:
        os.kill(syslogng_pid, signal.SIGTERM)
    except OSError:
        pass
    try:
        try:
            (pid, rc) = os.waitpid(syslogng_pid, 0)
        except OSError:
            raise
    finally:
        syslogng_pid = 0
    print_user("syslog-ng stopped")
    if rc == 0:
        return True
    print_user("syslog-ng exited with a non-zero value (%d)" % rc)
    return False

def flush_files(settle_time=3):
    global syslogng_pid

    if syslogng_pid == 0 or not messagegen.need_to_flush:
        return True

    print_user("waiting for syslog-ng to settle down before SIGHUP (%d secs)" % settle_time)
    # allow syslog-ng to settle
    time.sleep(settle_time)

    # sendMessages waits between etaps, so we assume that syslog-ng has
    # already received/processed everything we've sent to it. Go ahead send
    # a HUP signal.
    try:
        print_user("Sending syslog-ng the HUP signal (pid: %d)" % syslogng_pid)
        os.kill(syslogng_pid, signal.SIGHUP)
    except OSError:
        print_user("Error sending HUP signal to syslog-ng")
        raise
    # allow syslog-ng to perform config reload & file flush
    print_user("waiting for syslog-ng to process SIGHUP (%d secs)" % 2)
    time.sleep(2)
    messagegen.need_to_flush = False


def readpidfile(pidfile):
    f = open(pidfile, 'r')
    pid = f.read()
    f.close()
    return int(pid.strip())
