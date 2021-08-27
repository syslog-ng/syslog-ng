#!/usr/bin/env python
#############################################################################
# Copyright (c) 2020 One Identity
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
from pathlib2 import Path
from psutil import TimeoutExpired

import src.testcase_parameters.testcase_parameters as tc_parameters
from src.executors.process_executor import ProcessExecutor


class Loggen(object):

    instanceIndex = -1
    @staticmethod
    def __get_new_instance_index():
        Loggen.instanceIndex += 1
        return Loggen.instanceIndex

    def __init__(self):
        self.loggen_proc = None
        self.loggen_bin_path = tc_parameters.INSTANCE_PATH.get_loggen_bin()

    def __decode_start_parameters(
        self, inet, unix, stream, dgram, use_ssl, dont_parse, read_file, skip_tokens, loop_reading,
        rate, interval, permanent, syslog_proto, proxied, sdata, no_framing, active_connections,
        idle_connections, ipv6, debug, number, csv, quiet, size, reconnect, proxied_tls_passthrough,
        proxy_src_ip, proxy_dst_ip, proxy_src_port, proxy_dst_port,
    ):

        start_parameters = []

        if inet is True:
            start_parameters.append("--inet")

        if unix is True:
            start_parameters.append("--unix")

        if stream is True:
            start_parameters.append("--stream")

        if dgram is True:
            start_parameters.append("--dgram")

        if use_ssl is True:
            start_parameters.append("--use-ssl")

        if dont_parse is True:
            start_parameters.append("--dont-parse")

        if read_file is not None:
            start_parameters.append("--read-file={}".format(read_file))

        if skip_tokens is not None:
            start_parameters.append("--skip-tokens={}".format(skip_tokens))

        if loop_reading is True:
            start_parameters.append("--loop-reading")

        if rate is not None:
            start_parameters.append("--rate={}".format(rate))

        if interval is not None:
            start_parameters.append("--interval={}".format(interval))

        if permanent is True:
            start_parameters.append("--permanent")

        if syslog_proto is True:
            start_parameters.append("--syslog-proto")

        if proxied is True:
            start_parameters.append("--proxied")

        if proxy_src_ip is not None:
            start_parameters.append("--proxy-src-ip={}".format(proxy_src_ip))

        if proxy_dst_ip is not None:
            start_parameters.append("--proxy-dst-ip={}".format(proxy_dst_ip))

        if proxy_src_port is not None:
            start_parameters.append("--proxy-src-port={}".format(proxy_src_port))

        if proxy_dst_port is not None:
            start_parameters.append("--proxy-dst-port={}".format(proxy_dst_port))

        if proxied_tls_passthrough is True:
            start_parameters.append("--proxied-tls-passthrough")

        if sdata is True:
            start_parameters.append("--sdata")

        if no_framing is True:
            start_parameters.append("--no-framing")

        if active_connections is not None:
            start_parameters.append("--active-connections={}".format(active_connections))

        if idle_connections is not None:
            start_parameters.append("--idle-connections={}".format(idle_connections))

        if ipv6 is True:
            start_parameters.append("--ipv6")

        if debug is True:
            start_parameters.append("--debug")

        if number is not None:
            start_parameters.append("--number={}".format(number))

        if csv is True:
            start_parameters.append("--csv")

        if quiet is True:
            start_parameters.append("--quiet")

        if size is not None:
            start_parameters.append("--size={}".format(size))

        if reconnect is True:
            start_parameters.append("--reconnect")

        return start_parameters

    def start(
        self, target, port, inet=None, unix=None, stream=None, dgram=None, use_ssl=None, dont_parse=None, read_file=None, skip_tokens=None, loop_reading=None,
        rate=None, interval=None, permanent=None, syslog_proto=None, proxied=None, sdata=None, no_framing=None, active_connections=None,
        idle_connections=None, ipv6=None, debug=None, number=None, csv=None, quiet=None, size=None, reconnect=None, proxied_tls_passthrough=None,
        proxy_src_ip=None, proxy_dst_ip=None, proxy_src_port=None, proxy_dst_port=None,
    ):

        if self.loggen_proc is not None and self.loggen_proc.is_running():
            raise Exception("Loggen is already running, you shouldn't call start")

        instanceIndex = Loggen.__get_new_instance_index()
        self.loggen_stdout_path = Path(tc_parameters.WORKING_DIR, "loggen_stdout_{}".format(instanceIndex))
        self.loggen_stderr_path = Path(tc_parameters.WORKING_DIR, "loggen_stderr_{}".format(instanceIndex))

        self.parameters = self.__decode_start_parameters(
            inet, unix, stream, dgram, use_ssl, dont_parse, read_file, skip_tokens, loop_reading,
            rate, interval, permanent, syslog_proto, proxied, sdata, no_framing, active_connections,
            idle_connections, ipv6, debug, number, csv, quiet, size, reconnect, proxied_tls_passthrough,
            proxy_src_ip, proxy_dst_ip, proxy_src_port, proxy_dst_port,
        )

        self.loggen_proc = ProcessExecutor().start(
            [self.loggen_bin_path] + self.parameters + [target, port],
            self.loggen_stdout_path,
            self.loggen_stderr_path,
        )

        return self.loggen_proc

    def stop(self):
        if self.loggen_proc is None:
            return

        self.loggen_proc.terminate()
        try:
            self.loggen_proc.wait(4)
        except TimeoutExpired:
            self.loggen_proc.kill()

        self.loggen_proc = None

    def get_sent_message_count(self):
        if not self.loggen_stderr_path.exists():
            return 0

        # loggen puts the count= messages to the stderr
        f = open(str(self.loggen_stderr_path), "r")
        content = f.read()
        f.close()

        start_pattern = "count="
        if start_pattern not in content:
            return 0
        index_start = content.rindex(start_pattern) + len(start_pattern)

        end_pattern = ", "
        if end_pattern not in content:
            return 0
        index_end = content.find(end_pattern, index_start)

        return int(content[index_start:index_end])
