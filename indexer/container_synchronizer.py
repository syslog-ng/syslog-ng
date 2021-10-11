import logging
from pathlib import Path
from typing import List

from azure.storage.blob import ContainerClient

DEFAULT_ROOT_DIR = Path("/tmp")


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


class ContainerSynchronizer:
    def __init__(self, connection_string: str, container_name: str, sub_dir: Path) -> None:
        self.remote_dir = WorkingDir(Path(""), sub_dir)
        self.local_dir = WorkingDir(Path(DEFAULT_ROOT_DIR, container_name), sub_dir)
        self.__client = ContainerClient.from_connection_string(
            conn_str=connection_string, container_name=container_name
        )
        self.__logger = ContainerSynchronizer.__create_logger()

    @property
    def local_files(self) -> List[Path]:
        dirs_and_files = list(self.local_dir.working_dir.rglob("*"))
        return list(filter(lambda path: path.is_file(), dirs_and_files))

    @property
    def remote_files(self) -> List[dict]:
        file_name_prefix = "{}/".format(self.remote_dir.working_dir)
        return [dict(blob) for blob in self.__client.list_blobs(name_starts_with=file_name_prefix)]

    def __download_file(self, relative_file_path: str) -> None:
        download_path = Path(self.local_dir.root_dir, relative_file_path).resolve()

        self.__log_info("Downloading file.", remote_path=relative_file_path, local_path=str(download_path))

        download_path.parent.mkdir(parents=True, exist_ok=True)
        with download_path.open("wb") as downloaded_blob:
            blob_data = self.__client.download_blob(relative_file_path)
            blob_data.readinto(downloaded_blob)

    def __upload_file(self, relative_file_path: str) -> None:
        local_path = Path(self.local_dir.root_dir, relative_file_path)

        self.__log_info("Uploading file.", local_path=str(local_path), remote_path=relative_file_path)

        with local_path.open("rb") as local_file_data:
            self.__client.upload_blob(relative_file_path, local_file_data, overwrite=True)

    def __delete_local_file(self, relative_file_path: str) -> None:
        local_file_path = Path(self.local_dir.root_dir, relative_file_path).resolve()
        self.__log_info("Deleting local file.", local_path=str(local_file_path))
        local_file_path.unlink()

    def __delete_remote_file(self, relative_file_path: str) -> None:
        self.__log_info("Deleting remote file.", remote_path=relative_file_path)
        self.__client.delete_blob(relative_file_path)

    def sync_from_remote(self) -> None:
        # TODO: implement
        pass

    def sync_to_remote(self) -> None:
        # TODO: implement
        pass

    @staticmethod
    def __create_logger() -> logging.Logger:
        logger = logging.getLogger("ContainerSynchronizer")
        logger.setLevel(logging.INFO)
        return logger

    def __log_info(self, message: str, **kwargs: str) -> None:
        log = "[{} :: {}]\t{}".format(self.__client.container_name, str(self.remote_dir.working_dir), message)
        if len(kwargs) > 0:
            log += "\t{}".format(kwargs)
        self.__logger.info(log)
