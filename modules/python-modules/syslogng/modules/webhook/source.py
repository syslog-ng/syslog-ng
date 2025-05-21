#############################################################################
# Copyright (c) 2024 László Várady
# Copyright (c) 2025 One Identity LLC.
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

from syslogng import LogSource, LogMessage
from collections import defaultdict

import logging
import asyncio
import threading
import tornado.web
import ssl
import signal
import json
from typing import Any

signal.signal(signal.SIGINT, signal.SIG_IGN)
signal.signal(signal.SIGTERM, signal.SIG_IGN)
WEBHOOK_NV_PREFIX = "webhook."
WEBHOOK_QUERY_NV_PREFIX = WEBHOOK_NV_PREFIX + "query."
WEBHOOK_HEADERS_KEY = WEBHOOK_NV_PREFIX + "headers"


class Handler(tornado.web.RequestHandler):
    def initialize(self, source) -> None:
        self.source = source

    @tornado.web.authenticated
    async def post(self, **path_arguments) -> None:
        # racy, but the client should retry
        if self.source.suspended.is_set():
            self.set_status(503)
            await self.finish({"status": "flow-controlled"})
            return

        msg = self._construct_msg(self.request, path_arguments)
        self.source.post_message(msg)

        await self.finish({"status": "received"})

    def _set_proxied_ip(self, msg: LogMessage) -> None:
        proxy_headers = self.request.headers.get_list(self.source.proxy_header)

        if proxy_headers and len(proxy_headers) > 0:
            # the closest/last IP (the proxy_header flag implies that the last one can be trusted)
            msg.set_source_ipaddress(proxy_headers[-1])
            msg["PEERIP"] = self.request.remote_ip
            return

        msg.set_source_ipaddress(self.request.remote_ip)

    def _set_request_headers(self, msg: LogMessage) -> None:
        headers = defaultdict(list)
        for h in self.request.headers.get_all():
            name = h[0]
            if name:
                headers[name].append(h[1])

        msg[WEBHOOK_HEADERS_KEY] = json.dumps(headers)

    def _construct_msg(self, request, path_arguments) -> LogMessage:
        msg = LogMessage(self.request.body)
        msg.set_recvd_rawmsg_size(len(self.request.body))

        for key, value in request.query_arguments.items():
            value = value[0] if len(value) == 1 else value
            msg[WEBHOOK_QUERY_NV_PREFIX + key] = value

        for key, value in path_arguments.items():
            msg[key] = value

        if self.source.include_request_headers:
            self._set_request_headers(msg)

        if self.source.proxy_header:
            self._set_proxied_ip(msg)
        else:
            msg.set_source_ipaddress(self.request.remote_ip)

        return msg

    def set_default_headers(self) -> None:
        self.set_header("Server", "syslog-ng")

    def get_current_user(self):
        if not self.source.auth_token:
            return True

        token = self.request.headers.get("Authorization", "").split(" ")
        if len(token) != 2:
            self.source.logger.debug("Auth failed, missing Authorization header or auth-scheme")
            return False

        token = token[1]
        return self.source.auth_token == token

    def write_error(self, status_code: int, **kwargs: Any) -> None:
        self.set_status(status_code)


class HTTPSource(LogSource):
    def init(self, options: dict[str, Any]) -> bool:
        self.logger = logging.getLogger("http")
        self.set_transport_name("http")
        if not self.init_options(options):
            return False

        if not self.port:
            self.port = 443 if self.tls_key_file else 80
        self.port = int(self.port)

        self.ssl_ctx = None
        if self.tls_key_file:
            self.setup_tls()

        self.suspended = threading.Event()
        self.event_loop = asyncio.new_event_loop()
        self.request_exit = None

        async def init_request_exit():
            self.request_exit = asyncio.Event()
        self.event_loop.run_until_complete(init_request_exit())

        handlers = list(map(lambda p: (p, Handler, {"source": self}), self.paths))
        if len(handlers) == 0:
            handlers = [(r"/.*", Handler, {"source": self})]

        self.app = tornado.web.Application(
            handlers,
            log_function=self.log_access,
            autoreload=False,
            compress_response=False,
        )
        self.server = None

        return True

    async def runServer(self) -> None:
        self.logger.info(f"HTTP(S) server started on port {self.port}")
        self.server = self.app.listen(self.port, decompress_request=True, ssl_options=self.ssl_ctx)
        await self.request_exit.wait()

        self.server.stop()
        await self.server.close_all_connections()

    async def stopServer(self) -> None:
        self.request_exit.set()

    def deinit(self) -> None:
        pass

    def run(self) -> None:
        self.event_loop.run_until_complete(self.runServer())

    def suspend(self) -> None:
        self.suspended.set()

    def wakeup(self) -> None:
        self.suspended.clear()

    def request_exit(self) -> None:
        asyncio.run_coroutine_threadsafe(self.stopServer(), self.event_loop)
        pass

    def log_access(self, req: tornado.web.RequestHandler) -> None:
        self.logger.debug(f"{req.get_status()} {req._request_summary()}")

    def setup_tls(self) -> None:
        self.ssl_ctx = ssl.create_default_context(ssl.Purpose.CLIENT_AUTH)
        self.ssl_ctx.load_cert_chain(certfile=self.tls_cert_file, keyfile=self.tls_key_file)

        if self.tls_peer_verify:
            self.logger.debug("Enabling client cert verification")
            self.ssl_ctx.verify_mode = ssl.CERT_REQUIRED
        else:
            self.ssl_ctx.verify_mode = ssl.CERT_NONE

        if self.tls_use_system_cert_store:
            self.logger.debug("Using system cert store for client auth")
            self.ssl_ctx.load_default_certs(ssl.Purpose.CLIENT_AUTH)

        self.tls_ca_dir = self.tls_ca_dir if self.tls_ca_dir else None
        self.tls_ca_file = self.tls_ca_file if self.tls_ca_file else None

        if self.tls_ca_dir or self.tls_ca_file:
            self.ssl_ctx.load_verify_locations(cafile=self.tls_ca_file, capath=self.tls_ca_dir)

    def init_options(self, options: dict[str, Any]) -> bool:
        try:
            self.port = options.get("port")
            self.auth_token = options.get("auth_token")
            self.paths = options.get("paths", [])
            if isinstance(self.paths, str):
                self.paths = [self.paths]

            self.tls_key_file = options.get("tls_key_file")
            self.tls_cert_file = options.get("tls_cert_file")

            self.tls_peer_verify = bool(options.get("tls_peer_verify", False))
            self.tls_use_system_cert_store = bool(options.get("tls_use_system_cert_store", False))
            self.tls_ca_file = options.get("tls_ca_file")
            self.tls_ca_dir = options.get("tls_ca_dir")

            self.proxy_header = options.get("proxy_header")
            if self.proxy_header == "yes":
                self.proxy_header = "x-forwarded-for"

            self.include_request_headers = bool(options.get("include_request_headers", False))

            return True
        except KeyError as e:
            self.logger.error(f"Missing option '{e.args[0]}'")
            return False
