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

import logging
from abc import ABC, abstractmethod
from pathlib import Path

from cdn import CDN
from remote_storage_synchronizer import RemoteStorageSynchronizer


class Indexer(ABC):
    def __init__(
        self,
        incoming_remote_storage_synchronizer: RemoteStorageSynchronizer,
        incoming_sub_dir: Path,
        indexed_remote_storage_synchronizer: RemoteStorageSynchronizer,
        indexed_sub_dir: Path,
        cdn: CDN,
    ) -> None:
        self.__incoming_remote_storage_synchronizer = incoming_remote_storage_synchronizer
        self.__indexed_remote_storage_synchronizer = indexed_remote_storage_synchronizer
        self.__incoming_remote_storage_synchronizer.set_sub_dir(incoming_sub_dir)
        self.__indexed_remote_storage_synchronizer.set_sub_dir(indexed_sub_dir)

        self.__cdn = cdn
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
        path = Path(self.__indexed_remote_storage_synchronizer.remote_dir.working_dir, "*")
        self.__cdn.refresh_cache(path)

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
