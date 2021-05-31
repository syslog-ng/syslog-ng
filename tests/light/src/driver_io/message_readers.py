#!/usr/bin/env python
#############################################################################
# Copyright (c) 2022 One Identity
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
import asyncio

from src.common.asynchronous import BackgroundEventLoop


class SingleLineStreamReader(object):
    def __init__(self, stream):
        self._stream = stream

    def wait_for_messages(self, lines, timeout):
        return BackgroundEventLoop().wait_async_result(self._read_until_lines_found(lines, timeout=timeout))

    def wait_for_number_of_messages(self, number_of_lines, timeout):
        return BackgroundEventLoop().wait_async_result(self._read_number_of_lines(number_of_lines, timeout=timeout))

    async def _read_until_lines_found(self, lines, timeout):
        read_lines = []
        lines_to_find = lines.copy()

        while len(lines_to_find) > 0:
            try:
                line = await asyncio.wait_for(self._stream.readline(), timeout=timeout)
            except (asyncio.TimeoutError) as err:
                raise Exception("Could not find all lines. Remaining number of lines to find: {} Number of lines found: {}".format(len(lines_to_find), len(read_lines))) from err

            line = line.decode("utf-8")
            read_lines.append(line)
            _list_remove_partially_matching_element(lines_to_find, line)
        return read_lines

    async def _read_number_of_lines(self, number_of_lines, timeout):
        lines = []
        for i in range(number_of_lines):
            try:
                line = await asyncio.wait_for(self._stream.readline(), timeout=timeout)
            except (asyncio.TimeoutError) as err:
                raise Exception("Could not read {} number of lines. Connection closed after {} lines".format(number_of_lines, len(lines))) from err

            lines.append(line.decode("utf-8"))
        return lines


class DatagramReader(object):
    def __init__(self, datagram_server):
        self._datagram_server = datagram_server

    def wait_for_messages(self, lines, timeout, maxsize=65536):
        return BackgroundEventLoop().wait_async_result(self._read_until_dgrams_found(lines, maxsize, timeout=timeout))

    def wait_for_number_of_messages(self, number_of_msgs, timeout, maxsize=65536):
        return BackgroundEventLoop().wait_async_result(self._read_number_of_dgrams(number_of_msgs, maxsize, timeout=timeout))

    async def _read_until_dgrams_found(self, dgrams, maxsize, timeout):
        read_dgrams = []
        dgrams_to_find = dgrams.copy()

        while len(dgrams_to_find) != 0:
            try:
                dgram = await asyncio.wait_for(self._datagram_server.read_dgram(maxsize), timeout=timeout)
            except (asyncio.TimeoutError) as err:
                raise Exception("Could not find all datagrams. Remaining dgrams to find: {} Lines found: {}".format(len(dgrams_to_find), len(read_dgrams))) from err

            dgram = dgram.decode("utf-8")
            read_dgrams.append(dgram)
            _list_remove_partially_matching_element(dgrams_to_find, dgram)
        return read_dgrams

    async def _read_number_of_dgrams(self, number_of_dgrams, maxsize, timeout):
        msgs = []
        for i in range(number_of_dgrams):
            try:
                msg = await asyncio.wait_for(self._datagram_server.read_dgram(maxsize), timeout=timeout)
            except (asyncio.TimeoutError) as err:
                raise Exception("Could not read {} number of datagrams. Connection closed after {}".format(number_of_dgrams, len(msgs))) from err

            msgs.append(msg.decode("utf-8"))
        return msgs


class FramedStreamReader(object):
    """RFC6587 message reader for the syslog() driver"""
    # TODO msg_length = readuntil(' '); msg = readexactly(msg_length)
    pass


# FileIO and this method too operate on partial string matches
# TODO: LogMessage instances should be used instead, matching the full msg body
def _list_remove_partially_matching_element(list, elem):
    for l in list:
        if l in elem:
            list.remove(l)
            return True
    return False
