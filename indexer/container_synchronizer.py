from pathlib import Path
from typing import List


class ContainerSynchronizer:
    def __init__(self, connection_string: str, container_name: str, sub_dir: Path) -> None:
        # TODO: implement
        pass

    @property
    def local_files(self) -> List[Path]:
        # TODO: implement
        pass

    @property
    def remote_files(self) -> List[dict]:
        # TODO: implement
        pass

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
