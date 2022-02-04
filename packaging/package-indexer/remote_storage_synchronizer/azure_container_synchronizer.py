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

from hashlib import md5
from pathlib import Path
from typing import List, Optional

from azure.storage.blob import BlobClient, ContainerClient

from .remote_storage_synchronizer import FileSyncState, RemoteStorageSynchronizer

DEFAULT_ROOT_DIR = Path("/tmp/azure_container_synchronizer")


class AzureContainerSynchronizer(RemoteStorageSynchronizer):
    """
    A `RemoteStorageSynchronizer` implementation that can connect to an Azure Blob Container Storage instance.

    Example config:

    ```yaml
    vendor: "azure"
    incoming:
      all:
        storage-name: "incoming"
        connection-string: "secret1"
    indexed:
      stable:
        storage-name: "indexed"
        connection-string: "secret2"
      nightly:
        storage-name: "indexed"
        connection-string: "secret3"
    ```
    """

    def __init__(self, connection_string: str, storage_name: str) -> None:
        self.__client = ContainerClient.from_connection_string(conn_str=connection_string, container_name=storage_name)
        self.__remote_files_cache: Optional[List[dict]] = None
        super().__init__(
            remote_root_dir=Path(""),
            local_root_dir=Path(DEFAULT_ROOT_DIR, storage_name),
        )

    @staticmethod
    def get_config_keyword() -> str:
        return "azure"

    @staticmethod
    def from_config(cfg: dict) -> RemoteStorageSynchronizer:
        return AzureContainerSynchronizer(
            connection_string=cfg["connection-string"],
            storage_name=cfg["storage-name"],
        )

    @property
    def local_files(self) -> List[Path]:
        dirs_and_files = list(self.local_dir.working_dir.rglob("*"))
        return list(filter(lambda path: path.is_file(), dirs_and_files))

    @property
    def remote_files(self) -> List[dict]:
        if self.__remote_files_cache is not None:
            return self.__remote_files_cache

        file_name_prefix = "{}/".format(self.remote_dir.working_dir)
        self.__remote_files_cache = [dict(blob) for blob in self.__client.list_blobs(name_starts_with=file_name_prefix)]

        return self.__remote_files_cache

    def __download_file(self, relative_file_path: str) -> None:
        download_path = Path(self.local_dir.root_dir, relative_file_path).resolve()

        self._log_info(
            "Downloading file.",
            remote_path=relative_file_path,
            local_path=str(download_path),
        )

        download_path.parent.mkdir(parents=True, exist_ok=True)
        with download_path.open("wb") as downloaded_blob:
            blob_data = self.__client.download_blob(relative_file_path)
            blob_data.readinto(downloaded_blob)

    def __upload_file(self, relative_file_path: str) -> None:
        local_path = Path(self.local_dir.root_dir, relative_file_path)

        self._log_info(
            "Uploading file.",
            local_path=str(local_path),
            remote_path=relative_file_path,
        )

        with local_path.open("rb") as local_file_data:
            self.__client.upload_blob(relative_file_path, local_file_data, overwrite=True)

    def __delete_local_file(self, relative_file_path: str) -> None:
        local_file_path = Path(self.local_dir.root_dir, relative_file_path).resolve()
        self._log_info("Deleting local file.", local_path=str(local_file_path))
        local_file_path.unlink()

    def __delete_remote_file(self, relative_file_path: str) -> None:
        self._log_info("Deleting remote file.", remote_path=relative_file_path)
        self.__client.delete_blob(relative_file_path, delete_snapshots="include")

    def sync_from_remote(self) -> None:
        self._log_info(
            "Syncing content from remote.",
            remote_workdir=str(self.remote_dir.working_dir),
            local_workdir=str(self.local_dir.working_dir),
        )
        for file in self.__all_files:
            sync_state = self.__get_file_sync_state(file)
            if sync_state == FileSyncState.IN_SYNC:
                continue
            if sync_state == FileSyncState.DIFFERENT or sync_state == FileSyncState.NOT_IN_LOCAL:
                self.__download_file(file)
                continue
            if sync_state == FileSyncState.NOT_IN_REMOTE:
                self.__delete_local_file(file)
                continue
            raise NotImplementedError("Unexpected FileSyncState: {}".format(sync_state))
        self._log_info(
            "Successfully synced remote content.",
            remote_workdir=str(self.remote_dir.working_dir),
            local_workdir=str(self.local_dir.working_dir),
        )

    def sync_to_remote(self) -> None:
        self._log_info(
            "Syncing content to remote.",
            local_workdir=str(self.local_dir.working_dir),
            remote_workdir=str(self.remote_dir.working_dir),
        )
        for file in self.__all_files:
            sync_state = self.__get_file_sync_state(file)
            if sync_state == FileSyncState.IN_SYNC:
                continue
            if sync_state == FileSyncState.DIFFERENT or sync_state == FileSyncState.NOT_IN_REMOTE:
                self.__upload_file(file)
                continue
            if sync_state == FileSyncState.NOT_IN_LOCAL:
                self.__delete_remote_file(file)
                continue
            raise NotImplementedError("Unexpected FileSyncState: {}".format(sync_state))
        self._log_info(
            "Successfully synced local content.",
            remote_workdir=str(self.remote_dir.working_dir),
            local_workdir=str(self.local_dir.working_dir),
        )
        self.__invalidate_remote_files_cache()

    def create_snapshot_of_remote(self) -> None:
        self._log_info("Creating snapshot of the remote container.")
        for file in self.remote_files:
            blob_client: BlobClient = self.__client.get_blob_client(file["name"])
            snapshot_properties = blob_client.create_snapshot()
            self._log_debug(
                "Successfully created snapshot of remote file.",
                remote_path=self.__get_relative_file_path_for_remote_file(file),
                snapshot_properties=snapshot_properties,
            )

    def __get_md5_of_remote_file(self, relative_file_path: str) -> bytearray:
        for file in self.remote_files:
            if file["name"] == relative_file_path:
                return file["content_settings"]["content_md5"]
        raise FileNotFoundError

    def __get_md5_of_local_file(self, relative_file_path: str) -> bytes:
        file = Path(self.local_dir.root_dir, relative_file_path)
        return md5(file.read_bytes()).digest()

    def __get_file_sync_state(self, relative_file_path: str) -> FileSyncState:
        try:
            remote_md5 = self.__get_md5_of_remote_file(relative_file_path)
        except FileNotFoundError:
            self._log_debug(
                "Local file is not available remotely.",
                local_path=str(Path(self.local_dir.root_dir, relative_file_path)),
                unavailable_remote_path=str(Path(self.remote_dir.root_dir, relative_file_path)),
            )
            return FileSyncState.NOT_IN_REMOTE

        try:
            local_md5 = self.__get_md5_of_local_file(relative_file_path)
        except FileNotFoundError:
            self._log_debug(
                "Remote file is not available locally.",
                remote_path=str(Path(self.remote_dir.root_dir, relative_file_path)),
                unavailable_local_path=str(Path(self.local_dir.root_dir, relative_file_path)),
            )
            return FileSyncState.NOT_IN_LOCAL

        if remote_md5 != local_md5:
            self._log_debug(
                "File differs locally and remotely.",
                remote_path=str(Path(self.remote_dir.root_dir, relative_file_path)),
                local_path=str(Path(self.local_dir.root_dir, relative_file_path)),
                remote_md5sum=remote_md5.hex(),
                local_md5sum=local_md5.hex(),
            )
            return FileSyncState.DIFFERENT

        self._log_debug(
            "File is in sync.",
            remote_path=str(Path(self.remote_dir.root_dir, relative_file_path)),
            local_path=str(Path(self.local_dir.root_dir, relative_file_path)),
            md5sum=remote_md5.hex(),
        )
        return FileSyncState.IN_SYNC

    def __get_relative_file_path_for_local_file(self, file: Path) -> str:
        return str(file.relative_to(self.local_dir.root_dir))

    def __get_relative_file_path_for_remote_file(self, file: dict) -> str:
        return file["name"]

    @property
    def __all_files(self) -> List[str]:
        files = set()
        for local_file in self.local_files:
            files.add(self.__get_relative_file_path_for_local_file(local_file))
        for remote_file in self.remote_files:
            files.add(self.__get_relative_file_path_for_remote_file(remote_file))
        return sorted(files)

    def __invalidate_remote_files_cache(self) -> None:
        self.__remote_files_cache = None

    def _prepare_log(self, message: str, **kwargs: str) -> str:
        log = "[{} :: {}]\t{}".format(self.__client.container_name, str(self.remote_dir.working_dir), message)
        return super()._prepare_log(log, **kwargs)
