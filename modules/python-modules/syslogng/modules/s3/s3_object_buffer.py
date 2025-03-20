#############################################################################
# Copyright (c) 2025 One Identity LLC
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
import json
from os import path, unlink
from queue import SimpleQueue, Full, Empty
from uuid import uuid4

from .compressable_file_buffer import CompressableFileBuffer

from pathlib import Path
from threading import Lock
from typing import Any, Dict, Callable
from logging import getLogger


class S3ObjectBuffer:

    logger = getLogger("S3.ObjectBuffer.S3ObjectBuffer")

    def __init__(self, working_dir: Path, object_key: str, target_index: int, target_timestamp: str, creator: str, object_settings: Dict[str, Any]) -> None:
        self.__buffer_lock = Lock()
        self.__meta = {
            "creator": creator,
            "object_key": object_key,
            "suffix": object_settings.get("suffix", ""),
            "target_index": target_index,
            "target_timestamp": target_timestamp,
            "storage_class": object_settings.get("storage_class", "STANDARD"),
            "compression": object_settings.get("compression", False),
            "compresslevel": object_settings.get("compresslevel", 0),
            "max_size": object_settings.get("max_object_size", None),
            "acl": object_settings.get("canned_acl", None),
            "content_type": object_settings.get("content_type", "application/octet-stream"),
        }
        self.__basename = f"{object_key}_{uuid4()}"
        self.__path = Path(working_dir, self.__basename)
        self.__meta_path = Path(working_dir, self.__basename + "_meta.json")
        self.retries = 0
        self.buffer = None
        self.__buffer_source = "pending"
        self.__finished = False

    def create_file(self):
        self.buffer = CompressableFileBuffer(self.__path, self.__meta["compression"], self.__meta["compresslevel"])
        with open(self.__meta_path, "w") as meta_file:
            meta_file.write(json.dumps(self.__meta))
        self.__buffer_source = "created"

    def load_metadata(self, meta_path: Path) -> None:
        self.__meta_path = meta_path
        self.__path = Path(str(meta_path).removesuffix("_meta.json"))
        with open(self.__meta_path, "r") as meta_file:
            self.__meta = self.__meta | json.loads(meta_file.read())
        self.buffer = CompressableFileBuffer(self.__path, self.__meta["compression"], self.__meta["compresslevel"])
        self.__buffer_source = "loaded"

    def finish(self):
        with self.__buffer_lock:
            self.__finish()

    def __finish(self):
        if not self.__finished:
            self.buffer.finish()
            self.buffer.close()
            self.__finished = True

    def write(self, msg: bytes) -> bool:
        if self.__finished:
            return False
        with self.__buffer_lock:
            if self.buffer is None:
                self.create_file()
            if not self.buffer.write(msg):
                return False
            if self.buffer.tell() >= self.__meta["max_size"]:
                self.__finish()
        return True

    @property
    def creator(self) -> str:
        return self.__meta["creator"]

    @property
    def path(self) -> Path:
        return self.__path

    @property
    def object_key(self) -> str:
        return self.__meta["object_key"]

    @property
    def full_target_key(self) -> str:
        if self.__meta["target_index"] == 0:
            full_key = self.__meta["object_key"] + self.__meta["suffix"]
        else:
            full_key = f"{self.__meta['object_key']}-{self.__meta['target_index']}{self.__meta['suffix']}"
        return full_key

    @property
    def target_timestamp(self) -> str:
        return self.__meta["target_timestamp"]

    @property
    def last_modified(self):
        return int(path.getmtime(self.__path))

    @property
    def buffer_created(self):
        return self.__buffer_source

    @property
    def finished(self):
        if self.buffer is None:
            return False
        else:
            return self.__finished

    def deprecate(self):
        with self.__buffer_lock:
            self.buffer.unlink()
            unlink(self.__meta_path)

    @property
    def target_index(self):
        return self.__meta["target_index"]

    @property
    def storage_class(self):
        return self.__meta["storage_class"]

    @property
    def acl(self):
        return self.__meta["acl"]

    @property
    def content_type(self):
        return self.__meta["content_type"]


class S3ObjectQueue:

    logger = getLogger("S3.ObjectBuffer.S3ObjectQueue")

    def __init__(self):
        self.queue: SimpleQueue[S3ObjectBuffer] = SimpleQueue()

    def enqueue(self, s3_object: S3ObjectBuffer, enqueue_cb: Callable[[], None] | None):
        try:
            self.queue.put(s3_object)
        except Full:
            self.logger.error("Object queue is full, could not enqueue.")
            return
        if enqueue_cb:
            enqueue_cb()

    def dequeue(self) -> S3ObjectBuffer | None:
        try:
            return self.queue.get_nowait()
        except Empty:
            return None
