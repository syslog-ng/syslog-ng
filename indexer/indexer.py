from abc import ABC, abstractmethod
from pathlib import Path


class Indexer(ABC):
    def __init__(self) -> None:
        # TODO: implement
        pass

    def __sync_from_remote(self) -> None:
        # TODO: implement
        pass

    @abstractmethod
    def _index_pkgs(self, incoming_dir: Path, indexed_dir: Path) -> None:
        pass

    @abstractmethod
    def _sign_pkgs(self, indexed_dir: Path) -> None:
        pass

    def __sync_to_remote(self) -> None:
        # TODO: implement
        pass

    def __refresh_cdn_cache(self) -> None:
        # TODO: implement
        pass

    def index(self) -> None:
        incoming_dir = Path()
        indexed_dir = Path()

        self.__sync_from_remote()

        self._index_pkgs(incoming_dir, indexed_dir)
        self._sign_pkgs(indexed_dir)

        self.__sync_to_remote()
        self.__refresh_cdn_cache()
