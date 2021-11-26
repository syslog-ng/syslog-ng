from pathlib import Path

import cdn
import remote_storage_synchronizer


class Config:
    def __init__(self, file_path: Path) -> None:
        # TODO: implement
        pass

    def create_incoming_remote_storage_synchronizer(
        self,
        suite: str,
    ) -> remote_storage_synchronizer.RemoteStorageSynchronizer:
        # TODO: implement
        pass

    def create_indexed_remote_storage_synchronizer(
        self,
        suite: str,
    ) -> remote_storage_synchronizer.RemoteStorageSynchronizer:
        # TODO: implement
        pass

    def create_cdn(self, suite: str) -> cdn.CDN:
        # TODO: implement
        pass

    def get_gpg_key_path(self) -> Path:
        # TODO: implement
        pass
