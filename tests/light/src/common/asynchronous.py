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
import concurrent
import threading


class BackgroundEventLoop(object):
    """Singleton event loop running in the background"""
    _instance = None

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super(BackgroundEventLoop, cls).__new__(cls)
            cls._instance._init()
        return cls._instance

    def _init(self):
        self.loop = asyncio.new_event_loop()
        self.thread = threading.Thread(target=self._run_event_loop_func, daemon=True)
        self.thread.start()

    def _run_event_loop_func(self):
        asyncio.set_event_loop(self.loop)
        self.loop.run_forever()

    def run_async(self, async_func):
        return asyncio.run_coroutine_threadsafe(async_func, self.loop)

    def wait_async_result(self, async_func, timeout=None):
        future = self.run_async(async_func)
        try:
            return future.result(timeout=timeout)
        except (asyncio.TimeoutError, TimeoutError, concurrent.futures.TimeoutError) as err:
            future.cancel()
            raise asyncio.TimeoutError("Timeout expired for background event. async_func={}, timeout={} sec".format(async_func, timeout)) from err

    def stop(self):
        self.loop.call_soon_threadsafe(self.loop.stop)
