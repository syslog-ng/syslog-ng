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

    @property
    def local_files(self) -> List[Path]:
        dirs_and_files = list(self.local_dir.working_dir.rglob("*"))
        return list(filter(lambda path: path.is_file(), dirs_and_files))

    @property
    def remote_files(self) -> List[dict]:
        file_name_prefix = "{}/".format(self.remote_dir.working_dir)
        return [dict(blob) for blob in self.__client.list_blobs(name_starts_with=file_name_prefix)]

    def __download_file(self, relative_file_path: str) -> None:
        # TODO: implement
        pass

    def __upload_file(self, relative_file_path: str) -> None:
        # TODO: implement
        pass

    def __delete_local_file(self, relative_file_path: str) -> None:
        # TODO: implement
        pass

    def __delete_remote_file(self, relative_file_path: str) -> None:
        # TODO: implement
        pass

    def sync_from_remote(self) -> None:
        # TODO: implement
        pass

    def sync_to_remote(self) -> None:
        # TODO: implement
        pass
