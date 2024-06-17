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

from .s3_object import S3Object, S3ObjectPersist, ConstructorError, PersistLoadError, AlreadyFinishedError

try:
    from boto3 import client, Session
    from botocore.credentials import create_assume_role_refresher, DeferredRefreshableCredentials
    from botocore.exceptions import ClientError, EndpointConnectionError

    deps_installed = True
except ImportError:
    deps_installed = False

from concurrent.futures import ThreadPoolExecutor
from glob import glob
from logging import getLogger
from pathlib import Path
from signal import signal, SIGINT, SIG_IGN
from sys import exc_info
from syslogng import LogDestination, LogMessage, LogTemplate, get_installation_path_for
from threading import Event, Timer
from time import monotonic, sleep
from typing import Any, Dict, Optional

signal(SIGINT, SIG_IGN)


class S3Destination(LogDestination):
    S3_OBJECT_TIMEOUT_INTERVAL_SECONDS = 60

    def __init_options(self, options: Dict[str, Any]) -> None:
        try:
            self.url = str(options["url"])
            self.bucket = str(options["bucket"])
            self.access_key = str(options["access_key"])
            self.secret_key = str(options["secret_key"])
            self.role = str(options["role"])
            self.object_key: LogTemplate = options["object_key"]
            self.object_key_timestamp: Optional[LogTemplate] = options["object_key_timestamp"]
            self.message_template: LogTemplate = options["template"]
            self.compression = bool(options["compression"])
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

        if str(self.object_key_timestamp) == "":
            self.object_key_timestamp = None

        if self.compresslevel < 0 or self.compresslevel > 9:
            self.logger.warn("compresslevel() must be an integer between 0 and 9. Using 9")
            self.compresslevel = 9

        if self.chunk_size < S3Object.MIN_CHUNK_SIZE_BYTES:
            self.logger.warn(f"chunk-size() must be at least {S3Object.MIN_CHUNK_SIZE_BYTES}. Using minimal value")
            self.chunk_size = S3Object.MIN_CHUNK_SIZE_BYTES

        if self.upload_threads < 1:
            self.logger.warn("upload-threads() must be a positive integer. Using 1")
            self.upload_threads = 1

        if self.max_pending_uploads < 1:
            self.logger.warn("max-pending-uploads() must be a positive integer. Using 1")
            self.max_pending_uploads = 1

        if self.flush_grace_period < 1:
            self.logger.warn("flush-grace-period() must be a positive integer. Using 1")
            self.flush_grace_period = 1

        if self.region == "":
            self.region = None

        if self.server_side_encryption != "" and self.server_side_encryption != "aws:kms":
            assert False, "server-side-encryption() supports only aws:kms"

        if self.server_side_encryption == "aws:kms" and self.kms_key == "":
            assert False, "kms-key() must be set when server-side-encryption() is aws:kms"

        if self.kms_key != "" and self.server_side_encryption == "":
            self.logger.warn("ignoring kms-key() as server-side-encryption() is disabled")
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
            self.logger.warn(
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
            self.logger.warn(
                f"Invalid canned-acl(). Valid values are: f{', '.join(sorted(VALID_CANNED_ACLS))} or empty. Using empty"
            )
            self.canned_acl = ""

    def init(self, options: Dict[str, Any]) -> bool:
        self.logger = getLogger("S3")

        if not deps_installed:
            self.logger.error(
                "Unable to start the Python based S3 destination, the required Python dependencies (`boto3` and/or `botocore`) are missing"
            )
            return False

        self.session: Optional[Session] = None
        self.client: Optional[Any] = None

        self.s3_objects: Dict[str, S3Object] = dict()
        self.finished_s3_objects: Dict[str, Dict[str, Dict[int, S3Object]]] = dict()
        self.backfill_s3_objects: Dict[str, Dict[str, S3Object]] = dict()

        self.__init_options(options)

        self.exit_requested = Event()
        self.executor = ThreadPoolExecutor(max_workers=self.upload_threads)
        self.flush_poll_event: Optional[Timer] = None

        self.working_dir = Path(get_installation_path_for(r"${localstatedir}"), "s3")
        self.persist_loaded = False

        return True

    def deinit(self) -> None:
        self.executor.shutdown()

    @staticmethod
    def generate_persist_name(options: Dict[str, Any]) -> str:
        return f"s3({','.join([options['url'], options['bucket'], str(options['object_key'])])})"

    def __load_persist(self) -> None:
        for path_str in glob(pathname="*.json", root_dir=self.working_dir):
            path = Path(self.working_dir, path_str)
            try:
                persist = S3ObjectPersist.load(path=path)
            except PersistLoadError:
                self.logger.error(f"Cannot load persist file: {str(path)}")
                continue

            if persist.persist_name != self.persist_name:
                continue

            try:
                s3_object = S3Object.load_finished(
                    persist=persist,
                    executor=self.executor,
                    client=self.client,
                    logger=self.logger,
                    exit_requested=self.exit_requested,
                )
            except ConstructorError as e:
                self.logger.error(str(e))
                continue

            assert s3_object.finished

            target_key = s3_object.target_key
            self.finished_s3_objects.setdefault(target_key, dict()).setdefault(s3_object.timestamp, dict())[
                s3_object.index
            ] = s3_object

            # Completely new target
            if target_key not in self.s3_objects:
                self.s3_objects[target_key] = s3_object
                continue

            # Existing target
            existing_s3_object = self.s3_objects[target_key]

            # Same target and timestamp as stored
            if existing_s3_object.timestamp == s3_object.timestamp:
                if existing_s3_object.index < s3_object.index:
                    self.s3_objects[target_key] = s3_object
                continue

            # Target for a fresher timestamp
            if existing_s3_object.timestamp < s3_object.timestamp:
                self.s3_objects[target_key] = s3_object
                continue

            # Target for an older timestamp
            continue

        self.persist_loaded = True

    def __create_bucket(self) -> bool:
        assert self.client
        try:
            self.client.create_bucket(Bucket=self.bucket)
        except (ClientError, EndpointConnectionError) as e:
            self.logger.error(f"Failed to create bucket ({self.bucket}): {e}")
            return False

        self.logger.info(f"Bucket ({self.bucket}) successfully created")
        return True

    def open(self) -> bool:
        if self.is_opened():
            return True

        self.session = Session(
            aws_access_key_id=self.access_key if self.access_key != "" else None,
            aws_secret_access_key=self.secret_key if self.secret_key != "" else None,
            region_name=self.region,
        )

        if self.role != "":
            # NOTE: The Session.set_credentials always creates a new Credentials object from the given keys.
            # NOTE: The DeferredRefreshableCredentials class is a child of RefreshableCredentials which is a
            # NOTE: child of the Credentials class.
            self.session._session._credentials = DeferredRefreshableCredentials(
                refresh_using=create_assume_role_refresher(
                    self.session.client("sts"),
                    {"RoleArn": self.role, "RoleSessionName": "syslog-ng"}
                ),
                method="sts-assume-role",
            )

            sts = self.session.client("sts")
            whoami = sts.get_caller_identity().get("Arn")
            self.logger.info(f"Using {whoami} to access the bucket")

        self.client = self.session.client(
            service_name="s3",
            endpoint_url=self.url if self.url != "" else None,
        )

        is_opened = False
        try:
            self.client.head_bucket(Bucket=self.bucket)
            is_opened = True
        except ClientError as e:
            if e.response["Error"]["Code"] == "404":
                self.logger.info(f"Bucket ({self.bucket}) does not exist, trying to create it")
                is_opened = self.__create_bucket()
        except EndpointConnectionError:
            pass

        if not is_opened:
            self.client = None
            return False

        if not self.persist_loaded:
            self.__load_persist()

        self.exit_requested.clear()

        if self.flush_poll_event is None:
            self.__start_flush_poll_timer()

        return True

    def is_opened(self) -> bool:
        return self.client is not None

    def close(self) -> None:
        if not self.is_opened():
            return

        assert self.client is not None

        self.exit_requested.set()

        if self.flush_poll_event is not None:
            self.flush_poll_event.cancel()
            self.flush_poll_event = None

        for s3_object in self.s3_objects.values():
            if not s3_object.finished:
                self.__finish_s3_object(s3_object)

        for backfill_timestamps in self.backfill_s3_objects.values():
            for s3_object in backfill_timestamps.values():
                if not s3_object.finished:
                    self.__finish_s3_object(s3_object)

        for finished_timestamps in self.finished_s3_objects.values():
            for indexes in finished_timestamps.values():
                for s3_object in indexes.values():
                    s3_object.wait_for_upload_to_complete()

        self.finished_s3_objects.clear()

        self.client.close()
        self.client = None

    def __start_flush_poll_timer(self) -> None:
        self.flush_poll_event = Timer(
            interval=self.S3_OBJECT_TIMEOUT_INTERVAL_SECONDS, function=self.__flush_timed_out_s3_objects
        )
        self.flush_poll_event.start()

    def __flush_timed_out_s3_objects(self) -> None:
        def should_flush_s3_object(s3_object: S3Object, now: float) -> bool:
            return not s3_object.finished and s3_object.modified_at + self.flush_grace_period * 60 <= now

        now = monotonic()

        for s3_object in self.s3_objects.values():
            if should_flush_s3_object(s3_object, now):
                self.__finish_s3_object(s3_object)

        for timestamps in self.backfill_s3_objects.values():
            for s3_object in timestamps.values():
                if should_flush_s3_object(s3_object, now):
                    self.__finish_s3_object(s3_object)

        self.__start_flush_poll_timer()

    def __format_target_key(self, msg: LogMessage) -> str:
        return self.object_key.format(msg, self.template_options)

    def __format_timestamp(self, msg: LogMessage) -> str:
        if self.object_key_timestamp is not None:
            return self.object_key_timestamp.format(msg, self.template_options)
        return ""

    def __create_initial_s3_object(self, target_key: str, timestamp: str) -> S3Object:
        return S3Object.create_initial(
            working_dir=self.working_dir,
            bucket=self.bucket,
            target_key=target_key,
            timestamp=timestamp,
            compress=self.compression,
            server_side_encryption=self.server_side_encryption,
            kms_key=self.kms_key,
            storage_class=self.storage_class,
            persist_name=self.persist_name,
            executor=self.executor,
            compresslevel=self.compresslevel,
            chunk_size=self.chunk_size,
            canned_acl=self.canned_acl,
            client=self.client,
            logger=self.logger,
            exit_requested=self.exit_requested,
        )

    def __finish_s3_object(self, s3_object: S3Object) -> None:
        s3_object.finish()
        d = self.finished_s3_objects.setdefault(s3_object.target_key, dict()).setdefault(s3_object.timestamp, dict())
        d[s3_object.index] = s3_object

    def __get_backfill_s3_object(self, target_key: str, timestamp: str) -> S3Object:
        backfill_target_s3_objects = self.backfill_s3_objects.setdefault(target_key, dict())

        # Completely new backfill
        if timestamp not in backfill_target_s3_objects:
            try:
                # Backfill for a previously finished target
                indexes = list(self.finished_s3_objects[target_key][timestamp].keys())
                s3_object = self.finished_s3_objects[target_key][timestamp][max(indexes)].create_next()
            except KeyError:
                # Backfill for an unprocessed target
                s3_object = self.__create_initial_s3_object(target_key, timestamp)
            backfill_target_s3_objects[timestamp] = s3_object
            return s3_object

        # Existing backfill
        s3_object = backfill_target_s3_objects[timestamp]

        if not s3_object.finished:
            return s3_object

        s3_object = backfill_target_s3_objects[timestamp] = s3_object.create_next()
        return s3_object

    def __get_s3_object(self, msg: LogMessage) -> S3Object:
        target_key = self.__format_target_key(msg)
        timestamp = self.__format_timestamp(msg)

        # Completely new target
        if target_key not in self.s3_objects:
            s3_object = self.s3_objects[target_key] = self.__create_initial_s3_object(target_key, timestamp)
            return s3_object

        # Existing target
        s3_object = self.s3_objects[target_key]

        # Same target and timestamp as stored
        if s3_object.timestamp == timestamp:
            if not s3_object.finished:
                return s3_object

            s3_object = self.s3_objects[target_key] = s3_object.create_next()
            return s3_object

        # Target for a fresher timestamp
        if s3_object.timestamp < timestamp:
            self.__finish_s3_object(s3_object)
            s3_object = self.s3_objects[target_key] = self.__create_initial_s3_object(target_key, timestamp)
            return s3_object

        # Target for an older timestamp
        return self.__get_backfill_s3_object(target_key, timestamp)

    def __ensure_free_space_in_executor_queue(self) -> bool:
        """Waits for 1 minute."""
        retries = 0

        while self.executor._work_queue.qsize() >= self.max_pending_uploads:
            if retries >= 600:
                self.logger.error("Upload queue is still full after 1 minute, reconnecting")
                return False

            if retries == 0:
                self.logger.info("Upload queue is full, waiting for available space")

            sleep(0.1)
            retries += 1

        return True

    def send(self, msg: LogMessage) -> int:
        if not self.__ensure_free_space_in_executor_queue():
            return self.NOT_CONNECTED

        try:
            s3_object = self.__get_s3_object(msg)
        except ConstructorError as e:
            self.logger.error(str(e))
            return self.NOT_CONNECTED

        data = self.message_template.format(msg, self.template_options).encode("utf-8")

        try:
            s3_object.write(data)
        except OSError as e:
            self.logger.error(f"Failed to write data: {e}")
            return self.ERROR
        except AlreadyFinishedError as e:
            self.logger.error(f"Failed to write data: {e}")
            self.__finish_s3_object(s3_object)
            return self.ERROR

        if s3_object.finished:
            # The S3 object finished itself after a successful write.
            self.__finish_s3_object(s3_object)

        if s3_object.size >= self.max_object_size:
            self.__finish_s3_object(s3_object)

        self.stats_written_bytes_add(len(data))
        return self.SUCCESS
