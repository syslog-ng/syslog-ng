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
import os
from abc import ABC, abstractmethod
from pathlib import Path

from remote_storage_synchronizer import RemoteStorageSynchronizer


class Indexer(ABC):
    def __init__(
        self,
        incoming_remote_storage_synchronizer: RemoteStorageSynchronizer,
        incoming_sub_dir: Path,
        indexed_remote_storage_synchronizer: RemoteStorageSynchronizer,
        indexed_sub_dir: Path,
    ) -> None:
        self.__incoming_remote_storage_synchronizer = incoming_remote_storage_synchronizer
        self.__indexed_remote_storage_synchronizer = indexed_remote_storage_synchronizer
        self._incoming_sub_dir = incoming_sub_dir
        self._indexed_sub_dir = indexed_sub_dir

        self.__logger = Indexer.__create_logger()

    def __sync_from_remote(self) -> None:
        self.__incoming_remote_storage_synchronizer.set_sub_dir(self._incoming_sub_dir)
        self.__indexed_remote_storage_synchronizer.set_sub_dir(self._indexed_sub_dir)
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

    def index(self) -> None:
        self.__sync_from_remote()

        incoming_dir = self.__incoming_remote_storage_synchronizer.local_dir.working_dir
        indexed_dir = self.__indexed_remote_storage_synchronizer.local_dir.working_dir

        self._prepare_indexed_dir(incoming_dir, indexed_dir)
        self._index_pkgs(indexed_dir)
        self._sign_pkgs(indexed_dir)
        self._create_static_htmls(indexed_dir)

        self.__create_snapshot_of_indexed()
        self.__sync_to_remote()

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

    # create static index.html files, because the backed doesn't support dynamic indexes to allow humans to browse the
    # package repository. We'll use external index generators, if they aren't found, the index generation will be skipped.
    def _create_static_htmls(self, indexed_dir: Path) -> None:
        indexers = ["/usr/local/bin/genindex.py", "/usr/local/bin/indexer.py"]

        # delete the old index.html files, to avoid false information in case index.html generation fails.
        self._log_info("Purging old index.htmls ...")
        for html in Path(indexed_dir).rglob("**/index.html"):
            html.unlink()
        for indexer in indexers:
            if os.path.exists(indexer):
                self._log_info("Generating static index.html files with %s at '%s'" % (indexer, indexed_dir))
                ret = os.system("python3 %s -r '%s'" % (indexer, indexed_dir))
                self._log_info("index.html generator exited with %s" % ret)
                break
