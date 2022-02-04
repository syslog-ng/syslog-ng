#############################################################################
# Copyright (c) 2022 One Identity
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

import logging
from abc import ABC, abstractmethod
from enum import Enum, auto
from pathlib import Path


class WorkingDir:
    def __init__(self, root_dir: Path) -> None:
        self.__root_dir = root_dir
        self.__working_dir = root_dir

    @property
    def root_dir(self) -> Path:
        return self.__root_dir

    @property
    def working_dir(self) -> Path:
        return self.__working_dir

    def set_sub_dir(self, sub_dir: Path) -> None:
        self.__working_dir = Path(self.__root_dir, sub_dir)


class FileSyncState(Enum):
    IN_SYNC = auto()
    DIFFERENT = auto()
    NOT_IN_REMOTE = auto()
    NOT_IN_LOCAL = auto()


class RemoteStorageSynchronizer(ABC):
    def __init__(self, remote_root_dir: Path, local_root_dir: Path) -> None:
        self.remote_dir = WorkingDir(remote_root_dir)
        self.local_dir = WorkingDir(local_root_dir)

        self.__logger = RemoteStorageSynchronizer.__create_logger()

    @staticmethod
    @abstractmethod
    def get_config_keyword() -> str:
        pass

    @staticmethod
    @abstractmethod
    def from_config(cfg: dict) -> RemoteStorageSynchronizer:
        pass

    @abstractmethod
    def sync_from_remote(self) -> None:
        pass

    @abstractmethod
    def sync_to_remote(self) -> None:
        pass

    @abstractmethod
    def create_snapshot_of_remote(self) -> None:
        pass

    def set_sub_dir(self, sub_dir: Path) -> None:
        self.remote_dir.set_sub_dir(sub_dir)
        self.local_dir.set_sub_dir(sub_dir)

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
