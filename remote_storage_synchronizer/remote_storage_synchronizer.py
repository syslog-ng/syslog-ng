import logging
from abc import ABC, abstractmethod
from enum import Enum, auto
from pathlib import Path


class WorkingDir:
    def __init__(self, root_dir: Path, sub_dir: Path) -> None:
        self.__root_dir = root_dir
        self.__working_dir = Path(root_dir, sub_dir)

    @property
    def root_dir(self) -> Path:
        return self.__root_dir

    @property
    def working_dir(self) -> Path:
        return self.__working_dir


class FileSyncState(Enum):
    IN_SYNC = auto()
    DIFFERENT = auto()
    NOT_IN_REMOTE = auto()
    NOT_IN_LOCAL = auto()


class RemoteStorageSynchronizer(ABC):
    def __init__(self, remote_root_dir: Path, local_root_dir: Path, sub_dir: Path) -> None:
        self.remote_dir = WorkingDir(remote_root_dir, sub_dir)
        self.local_dir = WorkingDir(local_root_dir, sub_dir)

        self.__logger = RemoteStorageSynchronizer.__create_logger()

    @abstractmethod
    def sync_from_remote(self) -> None:
        pass

    @abstractmethod
    def sync_to_remote(self) -> None:
        pass

    @abstractmethod
    def create_snapshot_of_remote(self) -> None:
        pass

    @staticmethod
    def __create_logger() -> logging.Logger:
        logger = logging.getLogger("RemoteStorageSynchronizer")
        logger.setLevel(logging.DEBUG)
        return logger

    def _prepare_log(self, message: str, **kwargs: str) -> str:
        if len(kwargs) > 0:
            message += "\t{}".format(kwargs)
        return message

    def _log_info(self, message: str, **kwargs: str) -> None:
        log = self._prepare_log(message, **kwargs)
        self.__logger.info(log)

    def _log_debug(self, message: str, **kwargs: str) -> None:
        log = self._prepare_log(message, **kwargs)
        self.__logger.debug(log)
