from abc import ABC, abstractmethod
from pathlib import Path

from .container_synchronizer import ContainerSynchronizer


class Indexer(ABC):
    INCOMING_CONTAINER_NAME = "secret"
    INDEXED_CONTAINER_NAME = "secret"

    def __init__(
        self,
        container_connection_string: str,
        incoming_container_sub_dir: Path,
        indexed_container_sub_dir: Path,
    ) -> None:
        self.__incoming_container = ContainerSynchronizer(
            connection_string=container_connection_string,
            container_name=Indexer.INCOMING_CONTAINER_NAME,
            sub_dir=incoming_container_sub_dir,
        )
        self.__indexed_container = ContainerSynchronizer(
            connection_string=container_connection_string,
            container_name=Indexer.INDEXED_CONTAINER_NAME,
            sub_dir=indexed_container_sub_dir,
        )

    def __sync_from_remote(self) -> None:
        self.__incoming_container.sync_from_remote()
        self.__indexed_container.sync_from_remote()

    @abstractmethod
    def _index_pkgs(self, incoming_dir: Path, indexed_dir: Path) -> None:
        pass

    @abstractmethod
    def _sign_pkgs(self, indexed_dir: Path) -> None:
        pass

    def __sync_to_remote(self) -> None:
        self.__incoming_container.sync_to_remote()
        self.__indexed_container.sync_to_remote()

    def __refresh_cdn_cache(self) -> None:
        # TODO: implement
        pass

    def index(self) -> None:
        incoming_dir = self.__incoming_container.local_dir.working_dir
        indexed_dir = self.__indexed_container.local_dir.working_dir

        self.__sync_from_remote()

        self._index_pkgs(incoming_dir, indexed_dir)
        self._sign_pkgs(indexed_dir)

        self.__sync_to_remote()
        self.__refresh_cdn_cache()
