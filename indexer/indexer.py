import logging
from abc import ABC, abstractmethod
from pathlib import Path

from azure.core.polling import LROPoller
from azure.identity import ClientSecretCredential
from azure.mgmt.cdn import CdnManagementClient
from azure.mgmt.cdn.models import PurgeParameters

from remote_storage_synchronizer import RemoteStorageSynchronizer


class Indexer(ABC):
    CDN_RESOURCE_GROUP_NAME = "secret"
    CDN_PROFILE_NAME = "secret"
    CDN_ENDPOINT_NAME = "secret"
    CDN_ENDPOINT_SUBSCRIPTION_ID = "secret"

    def __init__(
        self,
        incoming_remote_storage_synchronizer: RemoteStorageSynchronizer,
        incoming_sub_dir: Path,
        indexed_remote_storage_synchronizer: RemoteStorageSynchronizer,
        indexed_sub_dir: Path,
        cdn_credential: ClientSecretCredential,
    ) -> None:
        self.__incoming_remote_storage_synchronizer = incoming_remote_storage_synchronizer
        self.__indexed_remote_storage_synchronizer = indexed_remote_storage_synchronizer
        self.__incoming_remote_storage_synchronizer.set_sub_dir(incoming_sub_dir)
        self.__indexed_remote_storage_synchronizer.set_sub_dir(indexed_sub_dir)

        self.__cdn = CdnManagementClient(
            credential=cdn_credential,
            subscription_id=Indexer.CDN_ENDPOINT_SUBSCRIPTION_ID,
        )
        self.__logger = Indexer.__create_logger()

    def __sync_from_remote(self) -> None:
        self.__incoming_remote_storage_synchronizer.sync_from_remote()
        self.__indexed_remote_storage_synchronizer.sync_from_remote()

    @abstractmethod
    def _prepare_indexed_dir(self, incoming_dir: Path, indexed_dir: Path) -> None:
        pass

    @abstractmethod
    def _index_pkgs(self, indexed_dir: Path) -> None:
        pass

    @abstractmethod
    def _sign_pkgs(self, indexed_dir: Path) -> None:
        pass

    def __create_snapshot_of_indexed(self) -> None:
        self.__indexed_remote_storage_synchronizer.create_snapshot_of_remote()

    def __sync_to_remote(self) -> None:
        self.__incoming_remote_storage_synchronizer.sync_to_remote()
        self.__indexed_remote_storage_synchronizer.sync_to_remote()

    def __refresh_cdn_cache(self) -> None:
        path = str(Path("/", self.__indexed_remote_storage_synchronizer.remote_dir.working_dir, "*"))

        self._log_info("Refreshing CDN cache.", path=path)

        poller: LROPoller = self.__cdn.endpoints.begin_purge_content(
            resource_group_name=Indexer.CDN_RESOURCE_GROUP_NAME,
            profile_name=Indexer.CDN_PROFILE_NAME,
            endpoint_name=Indexer.CDN_ENDPOINT_NAME,
            content_file_paths=PurgeParameters(content_paths=[path]),
        )
        poller.wait()

        status = poller.status()
        if not status == "Succeeded":
            raise Exception("Failed to refresh CDN cache. status: {}".format(status))

        self._log_info("Successfully refreshed CDN cache.", path=path)

    def index(self) -> None:
        incoming_dir = self.__incoming_remote_storage_synchronizer.local_dir.working_dir
        indexed_dir = self.__indexed_remote_storage_synchronizer.local_dir.working_dir

        self.__sync_from_remote()

        self._prepare_indexed_dir(incoming_dir, indexed_dir)
        self._index_pkgs(indexed_dir)
        self._sign_pkgs(indexed_dir)

        self.__create_snapshot_of_indexed()
        self.__sync_to_remote()
        self.__refresh_cdn_cache()

    @staticmethod
    def __create_logger() -> logging.Logger:
        logger = logging.getLogger("Indexer")
        logger.setLevel(logging.INFO)
        return logger

    def _log_info(self, message: str, **kwargs: str) -> None:
        log = message
        if len(kwargs) > 0:
            log += "\t{}".format(kwargs)
        self.__logger.info(log)
