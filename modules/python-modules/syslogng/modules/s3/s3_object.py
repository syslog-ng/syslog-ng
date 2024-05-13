#############################################################################
# Copyright (c) 2023 Attila Szakacs
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

from __future__ import annotations

from .compressable_file_buffer import CompressableFileBuffer

try:
    from botocore.exceptions import ClientError, EndpointConnectionError
except ImportError:
    pass

from concurrent.futures import Future, ThreadPoolExecutor
from logging import Logger
from pathlib import Path
from threading import Event, Lock
from time import monotonic
from typing import Any, Dict, Optional, List, Set
from uuid import uuid4

import json


class PersistLoadError(Exception):
    pass


class ConstructorError(Exception):
    pass


class AlreadyFinishedError(Exception):
    pass


class S3Chunk:
    MAX_PART_NUMBER = 9999

    def __init__(
        self,
        part_number: Optional[int] = None,
        compress: bool = False,
        compresslevel: Optional[int] = None,
        working_dir: Optional[Path] = None,
        path: Optional[Path] = None,
        prev_chunk: Optional[S3Chunk] = None,
    ) -> None:
        if prev_chunk is not None:
            path = Path(prev_chunk.buffer.path.parent, str(uuid4()))
            self.__buffer = prev_chunk.buffer.create_next(path)
            self.__part_number = prev_chunk.part_number + 1
            return

        if path is not None:
            assert part_number is not None
            self.__buffer = CompressableFileBuffer.load_finished(path=path)
            self.__part_number = part_number
            return

        assert working_dir is not None
        path = Path(working_dir, str(uuid4()))

        assert compress is not None and compresslevel is not None and part_number is not None
        self.__buffer = CompressableFileBuffer.create_initial(path=path, compress=compress, compresslevel=compresslevel)
        self.__part_number = part_number

    @staticmethod
    def create_initial(working_dir: Path, compress: bool, compresslevel: int) -> S3Chunk:
        return S3Chunk(working_dir=working_dir, part_number=1, compress=compress, compresslevel=compresslevel)

    @staticmethod
    def load_finished(path: Path, part_number: int) -> S3Chunk:
        return S3Chunk(path=path, part_number=part_number)

    def create_next(self) -> S3Chunk:
        return S3Chunk(prev_chunk=self)

    @property
    def buffer(self) -> CompressableFileBuffer:
        return self.__buffer

    @property
    def part_number(self) -> int:
        return self.__part_number


class S3ObjectPersist:
    """Thread safe persist class for the S3Object class."""

    def __init__(
        self,
        working_dir: Optional[Path] = None,
        path: Optional[Path] = None,
        persist_name: Optional[str] = None,
        bucket: Optional[str] = None,
        target_key: Optional[str] = None,
        timestamp: Optional[str] = None,
        compress: Optional[bool] = None,
        compresslevel: Optional[int] = None,
        chunk_size: Optional[int] = None,
        storage_class: Optional[str] = None,
        canned_acl: Optional[str] = None,
    ):
        if path is not None:
            self.__path = path
        elif working_dir is not None:
            self.__path = Path(working_dir, f"{str(uuid4())}.json")
            assert (
                persist_name is not None
                and bucket is not None
                and target_key is not None
                and timestamp is not None
                and compress is not None
                and compresslevel is not None
                and chunk_size is not None
                and storage_class is not None
                and canned_acl is not None
            )
        else:
            assert False

        self.__path.resolve().parent.mkdir(parents=True, exist_ok=True)
        self.__path.touch()

        self.__lock = Lock()

        cache: Dict[str, Any] = dict()
        with self.__path.open("r", encoding="utf-8") as f:
            try:
                cache = json.load(f)
            except json.decoder.JSONDecodeError as e:
                if path:
                    raise PersistLoadError from e

        if path:
            for field in {
                "persist-name",
                "bucket",
                "target-key",
                "timestamp",
                "index",
                "compress",
                "compresslevel",
                "chunk-size",
                "storage-class",
                "finished",
                "upload-id",
                "uploaded-parts",
                "pending-parts",
            }:
                try:
                    cache[field]
                except KeyError as e:
                    raise PersistLoadError from e

        self.__persist_name: str = cache.get("persist-name", persist_name)
        self.__bucket: str = cache.get("bucket", bucket)
        self.__target_key: str = cache.get("target-key", target_key)
        self.__timestamp: str = cache.get("timestamp", timestamp)
        self.__index: int = cache.get("index", -1)
        self.__compress: bool = cache.get("compress", compress)
        self.__compresslevel: bool = cache.get("compresslevel", compresslevel)
        self.__chunk_size: bool = cache.get("chunk-size", chunk_size)
        self.__storage_class: str = cache.get("storage-class", storage_class)
        self.__canned_acl: str = cache.get("canned-acl", canned_acl)
        self.__finished: bool = cache.get("finished", False)
        self.__upload_id: str = cache.get("upload-id", "")
        self.__uploaded_parts: List[Dict[str, Any]] = cache.get("uploaded-parts", [])
        self.__pending_parts: Dict[str, Any] = cache.get("pending-parts", dict())

        self.__flush()

    @staticmethod
    def load(path: Path) -> S3ObjectPersist:
        return S3ObjectPersist(path=path)

    def __flush(self) -> None:
        cache = {
            "persist-name": self.__persist_name,
            "bucket": self.__bucket,
            "target-key": self.__target_key,
            "timestamp": self.__timestamp,
            "index": self.__index,
            "compress": self.__compress,
            "compresslevel": self.__compresslevel,
            "storage-class": self.__storage_class,
            "canned-acl": self.__canned_acl,
            "chunk-size": self.__chunk_size,
            "finished": self.__finished,
            "upload-id": self.__upload_id,
            "uploaded-parts": self.__uploaded_parts,
            "pending-parts": self.__pending_parts,
        }

        with self.__path.open("w", encoding="utf-8") as f:
            try:
                json.dump(cache, f)
                f.flush()
            except TypeError:
                assert False

    def unlink(self):
        self.__path.unlink()

    @property
    def path(self) -> Path:
        return self.__path

    @property
    def persist_name(self) -> str:
        return self.__persist_name

    @property
    def bucket(self) -> str:
        return self.__bucket

    @property
    def target_key(self) -> str:
        return self.__target_key

    @property
    def timestamp(self) -> str:
        return self.__timestamp

    @property
    def index(self) -> int:
        with self.__lock:
            return self.__index

    @index.setter
    def index(self, i: int) -> None:
        with self.__lock:
            self.__index = i
            self.__flush()

    @property
    def compress(self) -> bool:
        return self.__compress

    @property
    def compresslevel(self) -> bool:
        return self.__compresslevel

    @property
    def chunk_size(self) -> bool:
        return self.__chunk_size

    @property
    def storage_class(self) -> str:
        return self.__storage_class

    @property
    def canned_acl(self) -> str:
        return self.__canned_acl

    @property
    def finished(self) -> bool:
        with self.__lock:
            return self.__finished

    @finished.setter
    def finished(self, f: bool) -> None:
        with self.__lock:
            self.__finished = f
            self.__flush()

    @property
    def upload_id(self) -> str:
        with self.__lock:
            return self.__upload_id

    @upload_id.setter
    def upload_id(self, u: str) -> None:
        with self.__lock:
            self.__upload_id = u
            self.__flush()

    @property
    def pending_parts(self) -> Dict[str, Any]:
        """
        A shallow copy of the pending parts.
        """
        with self.__lock:
            return self.__pending_parts.copy()

    def add_pending_part(self, path: Path, part_number: int) -> None:
        with self.__lock:
            self.__pending_parts[str(path)] = {"number": part_number}
            self.__flush()

    def cancel_pending_part(self, path: Path) -> None:
        with self.__lock:
            del self.__pending_parts[str(path)]
            self.__flush()

    @property
    def uploaded_parts(self) -> List[Dict[str, Any]]:
        """
        A sorted shallow copy of the uploaded parts.
        Can be used directly at the finish_multipart_upload() call.
        """
        with self.__lock:
            self.__uploaded_parts = sorted(self.__uploaded_parts, key=lambda x: x["PartNumber"])
            return self.__uploaded_parts.copy()

    def part_upload_finished(self, path: Path, etag: str, part_number: int) -> None:
        with self.__lock:
            self.__uploaded_parts.append({"ETag": etag, "PartNumber": part_number})
            del self.__pending_parts[str(path)]
            self.__flush()


class S3Object:
    MIN_CHUNK_SIZE_BYTES = 5 * 1024 * 1024

    def __init__(
        self,
        executor: ThreadPoolExecutor,
        client: Any,
        logger: Logger,
        exit_requested: Event,
        bucket: Optional[str] = None,
        target_key: Optional[str] = None,
        timestamp: Optional[str] = None,
        target_index: Optional[int] = None,
        compress: Optional[bool] = None,
        chunk_size: Optional[int] = None,
        compresslevel: Optional[int] = None,
        storage_class: Optional[str] = None,
        canned_acl: Optional[str] = None,
        persist: Optional[S3ObjectPersist] = None,
        persist_name: Optional[str] = None,
        working_dir: Optional[Path] = None,
    ) -> None:
        self.__executor = executor
        self.__client = client
        self.__logger = logger
        self.__exit_requested = exit_requested

        self.__size: int = 0
        self.__modified_at: float = monotonic()

        # Locked variables
        self.__lock = Lock()
        self.__current_chunk: Optional[S3Chunk] = None
        self.__upload_futures: List[Future] = []
        self.__multipart_completed: bool = False
        self.__multipart_complete_future: Optional[Future] = None

        if persist is None:
            assert (
                persist_name is not None
                and working_dir is not None
                and bucket is not None
                and target_key is not None
                and timestamp is not None
                and target_index is not None
                and compress is not None
                and chunk_size is not None
                and compresslevel is not None
                and storage_class is not None
                and canned_acl is not None
            )
            self.__persist = S3ObjectPersist(
                working_dir=working_dir,
                persist_name=persist_name,
                bucket=bucket,
                target_key=target_key,
                timestamp=timestamp,
                compress=compress,
                compresslevel=compresslevel,
                chunk_size=chunk_size,
                storage_class=storage_class,
                canned_acl=canned_acl,
            )
            self.__persist.index = self.__get_next_index(target_index)

            if self.index == -1:
                self.__persist.unlink()
                raise ConstructorError(f"Failed to get next ID for: {self.bucket}/{self.target_key}")

            self.__current_chunk = S3Chunk.create_initial(
                working_dir=working_dir,
                compress=compress,
                compresslevel=compresslevel,
            )
            self.__persist.add_pending_part(self.__current_chunk.buffer.path, self.__current_chunk.part_number)
        else:
            self.__persist = persist
            if not self.__persist.finished and self.__persist.compress:
                self.__logger.error(
                    f"Loaded an unfinished compressed object: {self.__persist.bucket}/{self.key}. "
                    "It is probably in an unusable state. syslog-ng will still upload this file, "
                    "but expect problems, while decompressing the file."
                )

            if self.bucket == "" or self.index == -1 or self.target_key == "":
                raise ConstructorError(f"Failed to load persist values for: {self.bucket}/{self.target_key}")

            for path, data in self.__persist.pending_parts.items():
                chunk = S3Chunk.load_finished(path=Path(path), part_number=data["number"])
                self.__upload_chunk(chunk)

            self.__persist.finished = True
            self.__complete_multipart()

    @staticmethod
    def create_initial(
        working_dir: Path,
        bucket: str,
        target_key: str,
        timestamp: str,
        compress: bool,
        compresslevel: int,
        storage_class: str,
        canned_acl: str,
        persist_name: str,
        chunk_size: int,
        executor: ThreadPoolExecutor,
        client: Any,
        logger: Logger,
        exit_requested: Event,
    ) -> S3Object:
        return S3Object(
            working_dir=working_dir,
            executor=executor,
            client=client,
            logger=logger,
            bucket=bucket,
            target_key=target_key,
            timestamp=timestamp,
            target_index=0,
            compress=compress,
            compresslevel=compresslevel,
            storage_class=storage_class,
            canned_acl=canned_acl,
            chunk_size=chunk_size,
            persist_name=persist_name,
            exit_requested=exit_requested,
        )

    @staticmethod
    def load_finished(
        persist: S3ObjectPersist,
        executor: ThreadPoolExecutor,
        client: Any,
        logger: Logger,
        exit_requested: Event,
    ) -> S3Object:
        return S3Object(persist=persist, executor=executor, client=client, logger=logger, exit_requested=exit_requested)

    def create_next(self) -> S3Object:
        return S3Object(
            working_dir=self.__persist.path.parent,
            executor=self.__executor,
            client=self.__client,
            logger=self.__logger,
            bucket=self.bucket,
            target_key=self.target_key,
            timestamp=self.timestamp,
            target_index=self.index + 1,
            compress=self.__persist.compress,
            compresslevel=self.__persist.compresslevel,
            storage_class=self.__persist.storage_class,
            canned_acl=self.__persist.canned_acl,
            chunk_size=self.__persist.chunk_size,
            persist_name=self.__persist.persist_name,
            exit_requested=self.__exit_requested,
        )

    @staticmethod
    def format_key(target_key: str, timestamp: str, index: int, compression: bool) -> str:
        key = target_key
        key += f"-{timestamp}" if timestamp != "" else ""
        key += f"-{str(index)}" if index > 0 else ""
        key += ".gz" if compression else ""
        return key

    @property
    def bucket(self) -> str:
        return self.__persist.bucket

    @property
    def target_key(self) -> str:
        return self.__persist.target_key

    @property
    def key(self) -> str:
        return self.format_key(self.target_key, self.timestamp, self.index, self.__persist.compress)

    @property
    def timestamp(self) -> str:
        return self.__persist.timestamp

    @property
    def index(self) -> int:
        return self.__persist.index

    @property
    def finished(self) -> bool:
        return self.__persist.finished

    @property
    def size(self) -> int:
        return self.__size

    @property
    def modified_at(self) -> float:
        return self.__modified_at

    @property
    def uploaded_parts(self) -> List[Dict[str, Any]]:
        """
        Waits for part uploads to finish, then returns a shallow copy of the uploaded parts.
        Can be used directly at the finish_multipart_upload() function.
        """
        while True:
            with self.__lock:
                try:
                    future = self.__upload_futures.pop()
                except IndexError:
                    break
            future.result()
        return self.__persist.uploaded_parts

    def __ensure_multipart_upload_started(self) -> bool:
        if self.__persist.upload_id != "":
            return True

        try:
            if self.__persist.canned_acl != "":
                response = self.__client.create_multipart_upload(
                    Bucket=self.bucket,
                    Key=self.key,
                    StorageClass=self.__persist.storage_class,
                    ACL=self.__persist.canned_acl,
                )
            else:
                response = self.__client.create_multipart_upload(
                    Bucket=self.bucket,
                    Key=self.key,
                    StorageClass=self.__persist.storage_class,
                )
        except (ClientError, EndpointConnectionError) as e:
            self.__logger.error(f"Failed to create multipart upload: {self.bucket}/{self.key} => {e}")
            return False

        try:
            self.__persist.upload_id = response["UploadId"]
        except KeyError:
            self.__logger.error(
                f"Failed to create multipart upload: {self.bucket}/{self.key} => Unexpected response: {response}"
            )
            return False

        return True

    def __upload_chunk_cb(self, chunk: S3Chunk, is_retry: bool) -> None:
        if not self.__ensure_multipart_upload_started():
            self.__upload_chunk(chunk, is_retry=True)
            return

        try:
            response = self.__client.upload_part(
                Bucket=self.bucket,
                Key=self.key,
                UploadId=self.__persist.upload_id,
                PartNumber=chunk.part_number,
                Body=chunk.buffer.getvalue(),
            )
        except EndpointConnectionError as e:
            self.__logger.error(f"Failed to upload part: {self.bucket}/{self.key} ({chunk.part_number}) => {e}")
            self.__upload_chunk(chunk, is_retry=True)
            return
        except ClientError as e:
            self.__logger.error(
                f"Critical failure when trying to upload part, finishing S3 object: {self.bucket}/{self.key} => {e}"
            )
            self.__persist.cancel_pending_part(chunk.buffer.path)
            self.finish()
            return

        try:
            etag = response["ETag"]
        except KeyError:
            self.__logger.error(
                f"Failed to upload part: {self.bucket}/{self.key} ({chunk.part_number}) => Unexpected response: {response}"
            )
            self.__upload_chunk(chunk, is_retry=True)
            return

        if is_retry:
            self.__logger.info(
                f"Successfully uploaded part after retry: {self.bucket}/{self.key} ({chunk.part_number})"
            )

        self.__persist.part_upload_finished(chunk.buffer.path, etag, chunk.part_number)

        chunk.buffer.unlink()

    def __upload_chunk(self, chunk: S3Chunk, is_retry: bool = False) -> None:
        if is_retry and self.__exit_requested.is_set():
            self.__logger.debug(
                f"Exit requested, skipping chunk upload: {self.bucket}/{self.key} => "
                f"{chunk.part_number} - {chunk.buffer.path}"
            )
            return

        if self.finished and chunk.buffer.is_empty():
            self.__persist.cancel_pending_part(chunk.buffer.path)
            chunk.buffer.unlink()
            return

        future = self.__executor.submit(self.__upload_chunk_cb, chunk, is_retry)
        with self.__lock:
            self.__upload_futures.append(future)

    def write(self, data: bytes) -> None:
        """Raises OSError on write failure. Cannot be called from multiple threads."""
        with self.__lock:
            chunk = self.__current_chunk
        if chunk is None:
            raise AlreadyFinishedError()

        old_size = chunk.buffer.tell()
        chunk.buffer.write(data)
        new_size = chunk.buffer.tell()
        self.__size += new_size - old_size

        self.__modified_at = monotonic()

        if new_size > self.__persist.chunk_size:
            self.__lock.acquire()

            if self.__current_chunk is None:
                # finish() was called while we were writing the buffer, this can only happen in part upload failure.
                self.__lock.release()
                raise AlreadyFinishedError()

            if self.__current_chunk.part_number == S3Chunk.MAX_PART_NUMBER:
                self.__lock.release()
                self.finish()
                return

            self.__current_chunk = chunk.create_next()
            self.__persist.add_pending_part(self.__current_chunk.buffer.path, self.__current_chunk.part_number)

            self.__lock.release()

            self.__upload_chunk(chunk)

    def __abort_multipart(self) -> bool:
        assert self.__persist.upload_id

        try:
            self.__client.abort_multipart_upload(
                Bucket=self.bucket,
                Key=self.key,
                UploadId=self.__persist.upload_id,
            )
        except EndpointConnectionError as e:
            self.__logger.error(f"Failed to abort multipart upload: {self.bucket}/{self.key} => {e}")
            return False
        except ClientError as e:
            self.__logger.error(f"Failed to abort multipart upload: {self.bucket}/{self.key} => {e}")
            if e.response["Error"]["Code"] == "NoSuchUpload":
                return True
            return False

        return True

    def __complete_multipart_cb(self, is_retry: bool) -> None:
        uploaded_parts = self.uploaded_parts
        pending_parts = self.__persist.pending_parts

        if len(pending_parts) > 0:
            self.__logger.error(
                f"Failed to complete multipart upload: {self.bucket}/{self.key} => Part uploads still pending"
            )
            self.__complete_multipart(is_retry=True)
            return

        if len(uploaded_parts) == 0:
            assert self.__persist.upload_id == ""
            self.__persist.unlink()
            with self.__lock:
                self.__multipart_completed = True
            return

        assert self.__persist.upload_id

        try:
            self.__client.complete_multipart_upload(
                Bucket=self.bucket,
                Key=self.key,
                MultipartUpload={"Parts": uploaded_parts},
                UploadId=self.__persist.upload_id,
            )
        except EndpointConnectionError as e:
            self.__logger.error(f"Failed to complete multipart upload: {self.bucket}/{self.key} => {e}")
            self.__complete_multipart(is_retry=True)
            return
        except ClientError as e:
            if e.response["Error"]["Code"] in {"EntityTooSmall", "InvalidPart", "InvalidPartOrder", "NoSuchUpload"}:
                self.__logger.error(
                    f"Critical failure when trying to complete multipart upload, dropping S3 object: {self.bucket}/{self.key} => {e}"
                )

                if not self.__abort_multipart():
                    self.__complete_multipart(is_retry=True)
                    return

                self.__persist.unlink()
                with self.__lock:
                    self.__multipart_completed = True
                return

            self.__logger.error(f"Failed to complete multipart upload: {self.bucket}/{self.key} => {e}")
            self.__complete_multipart(is_retry=True)
            return

        if is_retry:
            self.__logger.info(f"Successfully completed multipart upload after retry: {self.bucket}/{self.key}")

        self.__persist.unlink()
        with self.__lock:
            self.__multipart_completed = True

    def __complete_multipart(self, is_retry: bool = False) -> None:
        if is_retry and self.__exit_requested.is_set():
            self.__logger.debug(f"Exit requested, skipping multipart upload completion: {self.bucket}/{self.key}")
            with self.__lock:
                self.__multipart_complete_future = None
            return

        with self.__lock:
            self.__multipart_complete_future = self.__executor.submit(self.__complete_multipart_cb, is_retry)

    def finish(self) -> None:
        if self.finished:
            return

        chunk: Optional[S3Chunk] = None
        with self.__lock:
            if self.__current_chunk:
                chunk = self.__current_chunk
                self.__current_chunk.buffer.finish()
                self.__current_chunk = None

        if chunk is not None:
            self.__upload_chunk(chunk)

        self.__persist.finished = True
        self.__complete_multipart()

    def wait_for_upload_to_complete(self) -> None:
        assert self.finished

        is_retry = False
        while True:
            with self.__lock:
                if self.__multipart_complete_future is None:
                    break
                if self.__multipart_completed:
                    break
                if is_retry and self.__exit_requested.is_set():
                    break
                future = self.__multipart_complete_future
            future.result()
            is_retry = True

    def __list_pending_multiparts_in_s3(self) -> Optional[Set[str]]:
        uploads: Set[str] = set()
        pagination_options: Dict[str, str] = {}

        while True:
            try:
                response: Dict[str, Any] = self.__client.list_multipart_uploads(
                    Bucket=self.bucket,
                    Prefix=self.target_key,
                    **pagination_options,
                )
            except (ClientError, EndpointConnectionError) as e:
                self.__logger.error(f"Failed to list multipart uploads: {self.bucket}/{self.key} => {e}")
                return None

            try:
                for obj in response.get("Uploads", []):
                    uploads.add(obj["Key"])
                if not response["IsTruncated"]:
                    break
                pagination_options = {
                        "KeyMarker": response["NextKeyMarker"],
                        "UploadIdMarker": response["NextUploadIdMarker"],
                }
            except KeyError:
                self.__logger.error(
                    f"Failed to list multipart uploads: {self.bucket}/{self.key} => Unexpected response: {response}"
                )
                return None

        return uploads

    def __list_existing_objects_in_s3(self) -> Optional[Set[str]]:
        objects: Set[str] = set()
        pagination_options: Dict[str, str] = {}

        while True:
            try:
                response: Dict[str, Any] = self.__client.list_objects(
                    Bucket=self.bucket,
                    Prefix=self.target_key,
                    **pagination_options,
                )
            except (ClientError, EndpointConnectionError) as e:
                self.__logger.error(f"Failed to list objects: {self.bucket}/{self.key} => {e}")
                return None

            try:
                for obj in response.get("Contents", []):
                    objects.add(obj["Key"])
                if not response["IsTruncated"]:
                    break
                pagination_options = {
                        "Marker": response["Contents"][-1]["Key"]
                }
            except KeyError:
                self.__logger.error(
                    f"Failed to list objects: {self.bucket}/{self.key} => Unexpected response: {response}"
                )
                return None

        return objects

    def __get_next_index(self, target_index: int) -> int:
        pending_multiparts = self.__list_pending_multiparts_in_s3()
        if pending_multiparts is None:
            return -1

        existing_objects = self.__list_existing_objects_in_s3()
        if existing_objects is None:
            return -1

        objects: Set[str] = pending_multiparts | existing_objects

        i = target_index
        while True:
            uncompressed_key = S3Object.format_key(self.target_key, self.timestamp, i, False)
            compressed_key = S3Object.format_key(self.target_key, self.timestamp, i, True)
            if uncompressed_key not in objects and compressed_key not in objects:
                return i
            i += 1
