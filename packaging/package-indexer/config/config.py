from pathlib import Path

import yaml

import cdn
import remote_storage_synchronizer

from typing import Type


class Config:
    """
    A class that helps to configure the indexer.

    The config must be written in yaml format.

    There are 3 mandatory fields:
      * `storage`: configures the incoming and indexed remote storage synchronizers
      * `cdn`: configures the CDN
      * `gpg-key-path`: configures the path of the GPG key, needed for signing

    There are multiple vendor implementations for `storage` and `cdn`.
    Choosing one must be done with the use of the `vendor` field.

    `storage` is further split to `incoming` and `indexed` fields.

    The next level for both `storage` and `cdn` is regarding the suite.
    There are 3 different settings here:
      * `all`: sets the options for both stable and nightly suites
      * `stable`: sets the options for the stable suite
      * `nightly`: sets the options for the nightly suite

    The specific vendor's options are custom for each implementation.
    These options must be set under the respective suite.

    Example config:
    ```yaml
    storage:
      vendor: "azure"
      incoming:
        all:
          storage-name: "incoming"
          connection-string: "storage-secret1"
      indexed:
        stable:
          storage-name: "indexed"
          connection-string: "storage-secret2"
        nightly:
          storage-name: "indexed"
          connection-string: "storage-secret3"

    cdn:
      vendor: "azure"
      all:
        resource-group-name: "cdn-secret1"
        profile-name: "cdn-secret2"
        endpoint-name: "cdn-secret3"
        endpoint-subscription-id: "cdn-secret4"
        tenant-id: "cdn-secret5"
        client-id: "cdn-secret6"
        client-secret: "cdn-secret7"

    gpg-key-path: "secret-path"
    ```
    """

    def __init__(self, file_path: Path) -> None:
        with Path(file_path).open("r") as f:
            self.__cfg: dict = yaml.safe_load(f)

    def create_incoming_remote_storage_synchronizer(
        self,
        suite: str,
    ) -> remote_storage_synchronizer.RemoteStorageSynchronizer:
        vendor = self.__cfg["storage"]["vendor"]
        suites = self.__cfg["storage"]["incoming"]

        cls = Config.__get_storage_implementation(vendor)
        options = Config.__get_options_for_suite(suites, suite)

        return cls.from_config(options)

    def create_indexed_remote_storage_synchronizer(
        self,
        suite: str,
    ) -> remote_storage_synchronizer.RemoteStorageSynchronizer:
        vendor = self.__cfg["storage"]["vendor"]
        suites = self.__cfg["storage"]["indexed"]

        cls = Config.__get_storage_implementation(vendor)
        options = Config.__get_options_for_suite(suites, suite)

        return cls.from_config(options)

    def create_cdn(self, suite: str) -> cdn.CDN:
        vendor = self.__cfg["cdn"]["vendor"]
        suites = self.__cfg["cdn"]

        cls = Config.__get_cdn_implementation(vendor)
        options = Config.__get_options_for_suite(suites, suite)

        return cls.from_config(options)

    def get_gpg_key_path(self) -> Path:
        return self.__cfg["gpg-key-path"]

    @staticmethod
    def __get_storage_implementation(vendor: str) -> Type[remote_storage_synchronizer.RemoteStorageSynchronizer]:
        try:
            return remote_storage_synchronizer.get_implementations()[vendor]
        except KeyError:
            raise Exception("Unexpected storage vendor: {}".format(vendor))

    @staticmethod
    def __get_cdn_implementation(vendor: str) -> Type[cdn.CDN]:
        try:
            return cdn.get_implementations()[vendor]
        except KeyError:
            raise Exception("Unexpected cdn vendor: {}".format(vendor))

    @staticmethod
    def __get_options_for_suite(cfg_sub_block: dict, suite: str) -> dict:
        if suite in cfg_sub_block.keys():
            return cfg_sub_block[suite]

        return cfg_sub_block["all"]
