#############################################################################
# Copyright (c) 2025 One Identity LLC
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
from boto3.s3.transfer import TransferConfig
from boto3.session import Session
from boto3 import client
from botocore.credentials import DeferredRefreshableCredentials, create_assume_role_refresher
from botocore.exceptions import ClientError, EndpointConnectionError

from .s3_object_buffer import S3ObjectQueue, S3ObjectBuffer

from concurrent.futures import ThreadPoolExecutor, Future, CancelledError
from logging import getLogger
from typing import Any, Dict, Set
from threading import Lock
import re


class S3SessionHandler:

    logger = getLogger("S3.SessionHandler")
    __max_retries = 10

    def __init__(self, max_concurrent_uploads: int, upload_threads: int, object_queue: S3ObjectQueue, bucket: str, role: str | None, s3_session_config: Dict[str, Any], s3_client_config: Dict[str, Any], s3_sse_config: Dict[str, Any], s3_transfer_config: TransferConfig) -> None:
        self.__bucket = bucket
        self.__role = role
        self.__client_config = s3_client_config
        if s3_sse_config.get("ServerSideEncryption", None) is not None:
            self.__client_config.update(s3_sse_config)
        self.__session = Session(**s3_session_config)
        self.__sts = None
        self.__client = None

        # NOTE: The Session.set_credentials always creates a new Credentials object from the given keys.
        # NOTE: The DeferredRefreshableCredentials class is a child of RefreshableCredentials which is a
        # NOTE: child of the Credentials class.
        # ALSO NOTE: This solution is ugly as hell. We should find a more elegant one.
        if role:
            self.__sts = client("sts")
            self.__session._session._credentials = DeferredRefreshableCredentials(
                refresh_using=create_assume_role_refresher(
                    self.__sts,
                    {"RoleArn": self.__role, "RoleSessionName": "syslog-ng"},
                ),
                method="sts-assume-role",
            )
            self.logger.info(f"Using {self.__sts.get_caller_identity().get('Arn')} for the current session.")

        self.__transfer_config = s3_transfer_config
        self.__object_queue = object_queue
        self.__max_concurrent_uploads = max_concurrent_uploads
        self.__upload_threads = upload_threads
        self.__thread_pool = ThreadPoolExecutor(max_workers=max_concurrent_uploads)
        self.__futures: set[Future] = set()
        self.__future_lock = Lock()
        self.__connection_lock = Lock()
        self.__upload_enabled = True

    def open_connection(self):
        with self.__connection_lock:
            self.__client = self.__session.client("s3", **self.__client_config)

    def close_connection(self):
        with self.__connection_lock:
            self.__client.close()
            self.__client = None

    def disable_upload(self):
        with self.__connection_lock:
            self.__upload_enabled = False

    def enable_upload(self):
        with self.__connection_lock:
            self.__upload_enabled = True

    @property
    def connection_open(self):
        with self.__connection_lock:
            return self.__client is not None

    def ready_bucket(self):
        try:
            self.__client.head_bucket(Bucket=self.__bucket)
            return True
        except ClientError as e:
            if e.response["Error"]["Code"] == "404":
                self.logger.info(f"Bucket ({self.__bucket}) does not exist, trying to create it")
            try:
                self.__client.create_bucket(Bucket=self.__bucket)
            except (ClientError, EndpointConnectionError) as e:
                self.logger.error(f"Failed to create bucket ({self.__bucket}): {e}")
                return False
            self.logger.info(f"Bucket ({self.__bucket}) successfully created")
            return True

    def upload_object(self, s3_object: S3ObjectBuffer) -> bool:
        if not self.connection_open:
            self.logger.error(f"Connection closed. Establishing client connection.")
            self.open_connection()
        rc = False
        if s3_object is None:
            return rc
        if not self.__upload_enabled:
            return rc
        try:
            self.__client.upload_file(Filename=s3_object.path, Bucket=self.__bucket, Key=s3_object.full_target_key, Config=self.__transfer_config, ExtraArgs={"StorageClass": s3_object.storage_class, "ACL": s3_object.acl, "ContentType": s3_object.content_type})
            rc = True
            self.logger.debug(f"Object {s3_object.full_target_key} successfully uploaded.")
            s3_object.deprecate()
        except ClientError as e:
            self.logger.error(f"Could not upload object {s3_object.path}, retries: {s3_object.retries}/{self.__max_retries}. Reason: {e}")
            s3_object.retries += 1
            if s3_object.retries < self.__max_retries:
                self.__object_queue.enqueue(s3_object, None)
                return rc
            self.logger.warning(f"Maximum numbers of retries exceeded for object {s3_object.path}")

        return rc

    def __upload_complete_cb(self, future: Future) -> None:
        try:
            future.result()
        except ClientError as e:
            self.logger.error(f"Could not upload object: {e}")
        self.__futures.remove(future)

    def upload_loop(self) -> None:
        while True:
            with self.__future_lock:
                if len(self.__futures) < self.__max_concurrent_uploads:
                    s3_object = self.__object_queue.dequeue()
                    if s3_object is None:
                        return
                    self.logger.debug(f"Available executor found for uploading object: {s3_object.full_target_key}")
                    future = self.__thread_pool.submit(self.upload_object, s3_object)
                    future.add_done_callback(self.__upload_complete_cb)
                    self.__futures.add(future)
                else:
                    return

    def trigger_upload(self) -> None:
        self.upload_loop()

    def wait_for_queue_empty(self, timeout) -> bool:
        with self.__connection_lock and self.__future_lock:
            if len(self.__futures) == 0:
                return True
            for future in set(self.__futures):
                try:
                    future.result(timeout)
                except TimeoutError:
                    self.logger.warning("Object upload timed out.")
                    if not future.cancel():
                        self.logger.error("Timed out object upload could not be cancelled.")
                except CancelledError:
                    self.logger.warning("Object upload already cancelled.")
                except Exception as e:
                    self.logger.error(f"Future could not complete. Reason: {e}")
            if len(self.__futures) == 0:
                return True
            else:
                self.logger.error("Could not finish some upload futures. Uploads will be retried on next syslog-ng startup.")
                return False

    def get_top_index_for_key(self, key: str, suffix: str) -> int | None:
        top_index = -1
        objects: Set[str] = set()
        pagination_options: Dict[str, str] = {}

        while True:
            try:
                response: Dict[str, Any] = self.__client.list_objects(
                    Bucket=self.__bucket,
                    Prefix=key,
                    **pagination_options,
                )
            except (ClientError, EndpointConnectionError) as e:
                self.logger.error(f"Failed to list objects matching prefix: {self.__bucket}/{key} => {e}")
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
                self.logger.error(f"Failed to list objects: {self.__bucket}/{key} => Unexpected response: {response}")
                return None

        for s3_object in objects:
            if s3_object == key + suffix:
                key_index = 0
            else:
                key_match = re.search(rf"\d+{suffix}$", s3_object)
                if key_match:
                    key_index = int(key_match.group().removesuffix(suffix))
                else:
                    key_index = -1
            if key_index >= top_index:
                top_index = key_index

        # NOTE: python3 does not have a max integer size, thus this should not result in an overflow error
        return top_index if top_index >= 0 else None
