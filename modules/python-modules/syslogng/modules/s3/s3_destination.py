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


try:
    from .s3_object_buffer import S3ObjectBuffer, S3ObjectQueue
    from .s3_session_handler import S3SessionHandler

    # NOTE: These are imports required to get to the part processing `deps_installed`
    from logging import getLogger
    from signal import signal, SIGINT, SIG_IGN
    from syslogng import LogDestination, LogMessage, LogTemplate, get_installation_path_for

    from os import listdir, mkdir
    from pathlib import Path
    from sys import exc_info
    from threading import Timer, Lock
    from time import time
    from typing import Any, Dict, Optional

    from boto3 import client, Session
    from boto3.s3.transfer import TransferConfig
    from botocore.credentials import create_assume_role_refresher, DeferredRefreshableCredentials
    from botocore.exceptions import ClientError, EndpointConnectionError

    deps_missing = None
    deps_installed = True
except ImportError as import_error:
    deps_missing = import_error.name
    deps_installed = False

signal(SIGINT, SIG_IGN)


class S3Destination(LogDestination):
    S3_OBJECT_TIMEOUT_INTERVAL_SECONDS = 60
    S3_OBJECT_MIN_CHUNK_SIZE = 5 * 1024 * 1024

    logger = getLogger("S3")

    def __init_options(self, options: Dict[str, Any]) -> None:
        try:
            self.url = str(options["url"])
            self.bucket = str(options["bucket"])
            self.access_key = str(options["access_key"])
            self.secret_key = str(options["secret_key"])
            self.role = str(options["role"])
            self.object_key: LogTemplate = options["object_key"]
            self.object_key_timestamp: Optional[LogTemplate] = options["object_key_timestamp"]
            self.object_key_suffix: Optional[str] = options["object_key_suffix"]
            self.message_template: LogTemplate = options["template"]
            self.compression = bool(options["compression"])
            if self.compression:
                self.object_key_suffix += ".gz"
            self.compresslevel = int(options["compresslevel"])
            self.chunk_size = int(options["chunk_size"])
            self.max_object_size = int(options["max_object_size"])
            self.upload_threads = int(options["upload_threads"])
            self.max_pending_uploads = int(options["max_pending_uploads"])
            self.flush_grace_period = int(options["flush_grace_period"])
            self.region: Optional[str] = str(options["region"])
            self.server_side_encryption = str(options["server_side_encryption"])
            self.kms_key = str(options["kms_key"])
            self.storage_class = str(options["storage_class"]).upper().replace("-", "_")
            self.canned_acl = str(options["canned_acl"]).lower().replace("_", "-")
        except KeyError:
            assert False, (
                f"S3: {str(exc_info()[1])[1:-1]}() option is missing. "
                "If you are using this driver via the python() destination, please use the s3() driver directly. "
                "The option validation and propagation should be done by the s3.conf SCL."
            )

        self.__persist_name = self.generate_persist_name(options)

        if str(self.object_key_timestamp) == "":
            self.object_key_timestamp = None

        if self.compresslevel < 0 or self.compresslevel > 9:
            self.logger.warning("compresslevel() must be an integer between 0 and 9. Using 9")
            self.compresslevel = 9

        if self.chunk_size < self.S3_OBJECT_MIN_CHUNK_SIZE:
            self.logger.warning(f"chunk-size() must be at least {self.S3_OBJECT_MIN_CHUNK_SIZE}. Using minimal value")
            self.chunk_size = self.S3_OBJECT_MIN_CHUNK_SIZE

        if self.upload_threads < 1:
            self.logger.warning("upload-threads() must be a positive integer. Using 1")
            self.upload_threads = 1

        if self.max_pending_uploads < 1:
            self.logger.warning("max-pending-uploads() must be a positive integer. Using 1")
            self.max_pending_uploads = 1

        if self.flush_grace_period < 1:
            self.logger.warning("flush-grace-period() must be a positive integer. Using 1")
            self.flush_grace_period = 1

        if self.region == "":
            self.region = None

        if self.server_side_encryption != "" and self.server_side_encryption != "aws:kms":
            assert False, "server-side-encryption() supports only aws:kms"

        if self.server_side_encryption == "aws:kms" and self.kms_key == "":
            assert False, "kms-key() must be set when server-side-encryption() is aws:kms"

        if self.kms_key != "" and self.server_side_encryption == "":
            self.logger.warning("ignoring kms-key() as server-side-encryption() is disabled")
            self.kms_key = ""

        VALID_STORAGE_CLASSES = {
            "STANDARD",
            "REDUCED_REDUNDANCY",
            "STANDARD_IA",
            "ONEZONE_IA",
            "INTELLIGENT_TIERING",
            "GLACIER",
            "DEEP_ARCHIVE",
            "OUTPOSTS",
            "GLACIER_IR",
            "SNOW",
        }
        if self.storage_class not in VALID_STORAGE_CLASSES:
            self.logger.warning(
                f"Invalid storage-class(). Valid values are: f{', '.join(sorted(VALID_STORAGE_CLASSES))}. Using STANDARD"
            )
            self.storage_class = "STANDARD"

        VALID_CANNED_ACLS = {
            "private",
            "public-read",
            "public-read-write",
            "aws-exec-read",
            "authenticated-read",
            "bucket-owner-read",
            "bucket-owner-full-control",
            "log-delivery-write",
        }
        if self.canned_acl != "" and self.canned_acl not in VALID_CANNED_ACLS:
            self.logger.warning(
                f"Invalid canned-acl(). Valid values are: f{', '.join(sorted(VALID_CANNED_ACLS))} or empty. Using empty"
            )
            self.canned_acl = ""

    def init(self, options: Dict[str, Any]) -> bool:
        if not deps_installed:
            if deps_missing:
                self.logger.error(
                    f"Unable to start the Python based S3 destination. The required module dependency '{deps_missing}' could not be found.'"
                )
            else:
                self.logger.error(
                    "Unable to start the Python based S3 destination, some required Python dependencies (likely `boto3` and/or `botocore`) are missing"
                )
            return False

        self.__init_options(options)

        self.s3_session_config: Dict[str, Any] = {
            "aws_access_key_id": self.access_key if self.access_key != "" else None,
            "aws_secret_access_key": self.secret_key if self.secret_key != "" else None,
            "region_name": self.region,
        }
        self.s3_client_config: Dict[str, Any] = {
            "endpoint_url": self.url if self.url != "" else None,
        }
        self.s3_sse_options: Dict[str, Any] = {
            "ServerSideEncryption": self.server_side_encryption if self.server_side_encryption != "" else None,
            "SSEKMSKeyId": self.kms_key if self.server_side_encryption != "" else None,
        }
        self.transfer_config = TransferConfig(
            multipart_threshold=int(self.chunk_size * 1.5),
            max_concurrency=self.upload_threads,
            multipart_chunksize=self.chunk_size,
            use_threads=True if self.upload_threads > 1 else False,
        )
        self.object_config: Dict[str, Any] = {
            "suffix": self.object_key_suffix,
            "storage_class": self.storage_class,
            "compression": self.compression,
            "compresslevel": self.compresslevel,
            "max_object_size": self.max_object_size,
            "canned_acl": self.canned_acl,
        }

        self.s3_object_ready_queue: S3ObjectQueue = S3ObjectQueue()
        self.s3_objects_active: Dict[str, S3ObjectBuffer] = {}
        self.__objects_lock = Lock()
        self.__indices_lock = Lock()
        self.__indices: Dict[str, int] = {}

        self.__session_handler: S3SessionHandler = S3SessionHandler(self.max_pending_uploads, self.upload_threads, self.s3_object_ready_queue, self.bucket, self.role, self.s3_session_config, self.s3_client_config, self.s3_sse_options, self.transfer_config)

        self.flush_poll_event: Optional[Timer] = None

        self.working_dir = Path(get_installation_path_for(r"${localstatedir}"), "s3")
        try:
            mkdir(self.working_dir)
        except FileExistsError:
            pass

        return True

    def deinit(self) -> None:
        pass

    @staticmethod
    def generate_persist_name(options: Dict[str, Any]) -> str:
        return f"s3({','.join([options['url'], options['bucket'], str(options['object_key'])])})"

    def open(self) -> bool:
        self.logger.debug("Opening S3 connection.")
        if self.__session_handler.connection_open:
            self.logger.debug("S3 connection already open.")
            return True

        try:
            self.__session_handler.open_connection()

            if not self.__session_handler.ready_bucket():
                return False

            if self.flush_poll_event is None:
                self.__start_flush_poll_timer()

        except EndpointConnectionError as e:
            self.logger.error(f"Could not connect to S3 endpoint {self.url}. Reason: {e}")

        self.__session_handler.enable_upload()

        return True

    def is_opened(self) -> bool:
        return self.__session_handler.connection_open

    def close(self) -> None:
        self.logger.debug(f"Closing S3 connection.")
        if not self.is_opened():
            self.logger.debug(f"S3 connection already closed.")
            return

        assert self.__session_handler.connection_open

        if self.flush_poll_event is not None:
            self.flush_poll_event.cancel()
            self.flush_poll_event = None

        self.__session_handler.disable_upload()
        self.__session_handler.close_connection()

    def __start_flush_poll_timer(self) -> None:
        self.flush_poll_event = Timer(
            interval=self.S3_OBJECT_TIMEOUT_INTERVAL_SECONDS, function=self.__flush_timed_out_s3_objects
        )
        self.flush_poll_event.start()

    def __flush_timed_out_s3_objects(self) -> None:
        self.logger.debug("Flushing timed out S3 objects.")

        def should_flush_s3_object(s3_target_object: S3ObjectBuffer, current_time: float) -> bool:
            return not s3_target_object.finished and s3_target_object.last_modified + self.flush_grace_period * 60 <= current_time

        # NOTE: This used to be a monotonic value, but comparison with unix timestamps proved to be problematic
        now = int(time())

        with self.__objects_lock:
            object_list = list(self.s3_objects_active.values())
            for s3_object in object_list:
                if should_flush_s3_object(s3_object, now):
                    self.__finish_s3_object(s3_object)
                    with self.__indices_lock:
                        last_index = self.__indices.pop(s3_object.object_key)
                        self.__indices[s3_object.object_key] = last_index

        self.__start_flush_poll_timer()

    def __format_target_key(self, msg: LogMessage) -> str:
        return self.object_key.format(msg, self.template_options)

    def __format_timestamp(self, msg: LogMessage) -> str:
        if self.object_key_timestamp is not None:
            return self.object_key_timestamp.format(msg, self.template_options)
        return ""

    def __finish_s3_object(self, s3_object: S3ObjectBuffer) -> None:
        if not s3_object.finished:
            s3_object.finish()
        self.s3_objects_active.pop(s3_object.object_key)
        self.s3_object_ready_queue.enqueue(s3_object, self.__session_handler.trigger_upload)

    def __update_target_index(self, target_key):
        with self.__objects_lock and self.__indices_lock:
            greatest_online_index = self.__session_handler.get_top_index_for_key(target_key, self.object_key_suffix)
            if greatest_online_index is None:
                self.logger.error(f"Could not get latest online index for target key {target_key}. Continuing with internal indexing only.")
                greatest_online_index = -1
            if target_key in self.__indices.keys():
                self.__indices[target_key] = max(self.__indices[target_key] + 1, greatest_online_index + 1)
            else:
                self.__indices[target_key] = greatest_online_index + 1

    def __get_s3_object(self, msg: LogMessage) -> S3ObjectBuffer:
        timestamp = self.__format_timestamp(msg)
        target_key = self.__format_target_key(msg) + timestamp

        with self.__objects_lock:
            # Completely new target
            if target_key not in self.s3_objects_active:
                self.logger.debug(f"Could not fetch S3 object, creating new one.")
                self.__update_target_index(target_key)
                with self.__indices_lock:
                    s3_object = self.s3_objects_active[target_key] = S3ObjectBuffer(self.working_dir, target_key, self.__indices[target_key], timestamp, self.__persist_name, self.object_config)
                return s3_object
            else:
                return self.s3_objects_active[target_key]

    def send(self, msg: LogMessage) -> int:

        s3_object = self.__get_s3_object(msg)

        data = self.message_template.format(msg, self.template_options).encode("utf-8")

        try:
            if not s3_object.write(data):
                self.logger.error(f"Failed to write data: {data}")
        except OSError as e:
            self.logger.error(f"Failed to write data: {e}")
            return self.ERROR

        if s3_object.finished:
            # The S3 object finished itself after a successful write.
            with self.__objects_lock:
                self.__finish_s3_object(s3_object)

        self.stats_written_bytes_add(len(data))
        return self.SUCCESS
