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

import bz2
import gzip
import lzma
import re
from pathlib import Path
from tempfile import TemporaryDirectory
from typing import List, Optional

from cdn import CDN
from remote_storage_synchronizer import RemoteStorageSynchronizer

from indexer import Indexer

from . import utils

CURRENT_DIR = Path(__file__).parent.resolve()


class DebIndexer(Indexer):
    def __init__(
        self,
        incoming_remote_storage_synchronizer: RemoteStorageSynchronizer,
        indexed_remote_storage_synchronizer: RemoteStorageSynchronizer,
        incoming_sub_dir: Path,
        dist_dir: Path,
        cdn: CDN,
        apt_conf_file_path: Path,
        gpg_key_path: Path,
        gpg_key_passphrase: Optional[str],
    ) -> None:
        self.__apt_conf_file_path = apt_conf_file_path
        self.__gpg_key_path = gpg_key_path.expanduser()
        self.__gpg_key_passphrase = gpg_key_passphrase
        super().__init__(
            incoming_remote_storage_synchronizer=incoming_remote_storage_synchronizer,
            indexed_remote_storage_synchronizer=indexed_remote_storage_synchronizer,
            incoming_sub_dir=incoming_sub_dir,
            indexed_sub_dir=Path("apt", "dists", dist_dir),
            cdn=cdn,
        )

    def __move_files_from_incoming_to_indexed(self, incoming_dir: Path, indexed_dir: Path) -> None:
        for file in filter(lambda path: path.is_file(), incoming_dir.rglob("*")):
            relative_path = file.relative_to(incoming_dir)
            platform = relative_path.parent
            file_name = relative_path.name

            new_path = Path(indexed_dir, platform, "binary-amd64", file_name)

            self._log_info("Moving file.", src_path=str(file), dst_path=str(new_path))

            new_path.parent.mkdir(parents=True, exist_ok=True)
            utils.move_file_without_overwrite(file, new_path)

    def _prepare_indexed_dir(self, incoming_dir: Path, indexed_dir: Path) -> None:
        self.__move_files_from_incoming_to_indexed(incoming_dir, indexed_dir)

    def __create_packages_files(self, indexed_dir: Path) -> None:
        base_command = ["apt-ftparchive", "packages"]
        # Need to exec the commands in the `apt` dir.
        # APT wants to have the `Filename` field in the `Packages` file to start with `dists`.
        dir = indexed_dir.parents[1]

        for pkg_dir in list(indexed_dir.rglob("binary-amd64")):
            relative_pkg_dir = pkg_dir.relative_to(dir)
            command = base_command + [str(relative_pkg_dir)]

            packages_file_path = Path(pkg_dir, "Packages")
            with packages_file_path.open("w") as packages_file:
                self._log_info("Creating `Packages` file.", packages_file_path=str(packages_file_path))
                utils.execute_command(command, dir=dir, stdout=packages_file)

            packages_gz_file_path = Path(pkg_dir, "Packages.gz")
            with packages_gz_file_path.open("wb") as packages_gz_file:
                gz_compressed_data = gzip.compress(packages_file_path.read_bytes())
                packages_gz_file.write(gz_compressed_data)

            packages_xz_file_path = Path(pkg_dir, "Packages.xz")
            with packages_xz_file_path.open("wb") as packages_xz_file:
                xz_compressed_data = lzma.compress(packages_file_path.read_bytes(), lzma.FORMAT_XZ)
                packages_xz_file.write(xz_compressed_data)

            packages_bz2_file_path = Path(pkg_dir, "Packages.bz2")
            with packages_bz2_file_path.open("wb") as packages_bz2_file:
                bz2_compressed_data = bz2.compress(packages_file_path.read_bytes())
                packages_bz2_file.write(bz2_compressed_data)

    def __create_release_file(self, indexed_dir: Path) -> None:
        command = ["apt-ftparchive", "release", "."]

        release_file_path = Path(indexed_dir, "Release")
        with release_file_path.open("w") as release_file:
            self._log_info("Creating `Release` file.", release_file_path=str(release_file_path))
            utils.execute_command(
                command,
                dir=indexed_dir,
                stdout=release_file,
                env={"APT_CONFIG": str(self.__apt_conf_file_path)},
            )

    def _index_pkgs(self, indexed_dir: Path) -> None:
        self.__create_packages_files(indexed_dir)
        self.__create_release_file(indexed_dir)

    @staticmethod
    def __add_gpg_security_params(command: list) -> list:
        assert command[0] == "gpg"

        return [
            "gpg",
            "--no-tty",
            "--no-options",
        ] + command[1:]

    def __add_gpg_passphrase_params_if_needed(self, command: list) -> list:
        if self.__gpg_key_passphrase is None:
            return command

        assert command[0] == "gpg"

        return [
            "gpg",
            "--batch",
            "--pinentry-mode",
            "loopback",
            "--passphrase-fd",
            "0",
        ] + command[1:]

    def __add_gpg_key_to_chain(self, gnupghome: str) -> None:
        command = ["gpg", "--import", str(self.__gpg_key_path)]
        command = self.__add_gpg_security_params(command)
        command = self.__add_gpg_passphrase_params_if_needed(command)
        env = {"GNUPGHOME": gnupghome}

        self._log_info("Adding GPG key to chain.", gpg_key_path=str(self.__gpg_key_path))
        utils.execute_command(command, env=env, input=self.__gpg_key_passphrase)

    def __create_release_gpg_file(self, release_file_path: Path, gnupghome: str) -> None:
        release_gpg_file_path = Path(release_file_path.parent, "Release.gpg")
        command = [
            "gpg",
            "--output",
            str(release_gpg_file_path),
            "--armor",
            "--detach-sign",
            "--sign",
            str(release_file_path),
        ]
        command = self.__add_gpg_security_params(command)
        command = self.__add_gpg_passphrase_params_if_needed(command)
        env = {"GNUPGHOME": gnupghome}

        if release_gpg_file_path.exists():
            self._log_info("Removing old `Release.gpg` file.", release_gpg_file_path=str(release_gpg_file_path))
            release_gpg_file_path.unlink()

        self._log_info(
            "Creating `Release.gpg` file.",
            release_file_path=str(release_file_path),
            release_gpg_file_path=str(release_gpg_file_path),
        )
        utils.execute_command(command, env=env, input=self.__gpg_key_passphrase)

    def __create_inrelease_file(self, release_file_path: Path, gnupghome: str) -> None:
        inrelease_file_path = Path(release_file_path.parent, "InRelease")
        command = [
            "gpg",
            "--output",
            str(inrelease_file_path),
            "--armor",
            "--sign",
            "--clearsign",
            str(release_file_path),
        ]
        command = self.__add_gpg_security_params(command)
        command = self.__add_gpg_passphrase_params_if_needed(command)
        env = {"GNUPGHOME": gnupghome}

        if inrelease_file_path.exists():
            self._log_info("Removing old `InRelease` file.", inrelease_file_path=str(inrelease_file_path))
            inrelease_file_path.unlink()

        self._log_info(
            "Creating `InRelease` file.",
            release_file_path=str(release_file_path),
            inrelease_file_path=str(inrelease_file_path),
        )
        utils.execute_command(command, env=env, input=self.__gpg_key_passphrase)

    def _sign_pkgs(self, indexed_dir: Path) -> None:
        gnupghome = TemporaryDirectory(dir=CURRENT_DIR)
        release_file_path = Path(indexed_dir, "Release")

        try:
            self.__add_gpg_key_to_chain(gnupghome.name)
            self.__create_release_gpg_file(release_file_path, gnupghome.name)
            self.__create_inrelease_file(release_file_path, gnupghome.name)
        finally:
            self._log_info("Cleaning up `GNUPGHOME` directory.", gnupghome=gnupghome.name)
            gnupghome.cleanup()


class StableDebIndexer(DebIndexer):
    def __init__(
        self,
        incoming_remote_storage_synchronizer: RemoteStorageSynchronizer,
        indexed_remote_storage_synchronizer: RemoteStorageSynchronizer,
        run_id: str,
        cdn: CDN,
        gpg_key_path: Path,
        gpg_key_passphrase: Optional[str],
    ) -> None:
        super().__init__(
            incoming_remote_storage_synchronizer=incoming_remote_storage_synchronizer,
            indexed_remote_storage_synchronizer=indexed_remote_storage_synchronizer,
            incoming_sub_dir=Path("stable", run_id),
            dist_dir=Path("stable"),
            cdn=cdn,
            apt_conf_file_path=Path(CURRENT_DIR, "apt_conf", "stable.conf"),
            gpg_key_path=gpg_key_path,
            gpg_key_passphrase=gpg_key_passphrase,
        )


class NightlyDebIndexer(DebIndexer):
    PKGS_TO_KEEP = 10

    def __init__(
        self,
        incoming_remote_storage_synchronizer: RemoteStorageSynchronizer,
        indexed_remote_storage_synchronizer: RemoteStorageSynchronizer,
        cdn: CDN,
        run_id: str,
        gpg_key_path: Path,
        gpg_key_passphrase: Optional[str],
    ) -> None:
        super().__init__(
            incoming_remote_storage_synchronizer=incoming_remote_storage_synchronizer,
            indexed_remote_storage_synchronizer=indexed_remote_storage_synchronizer,
            incoming_sub_dir=Path("nightly", run_id),
            dist_dir=Path("nightly"),
            cdn=cdn,
            apt_conf_file_path=Path(CURRENT_DIR, "apt_conf", "nightly.conf"),
            gpg_key_path=gpg_key_path,
            gpg_key_passphrase=gpg_key_passphrase,
        )

    def __get_pkg_timestamps_in_dir(self, dir: Path) -> List[str]:
        timestamp_regexp = re.compile(r"\+([^_]+)_")
        pkg_timestamps: List[str] = []

        for deb_file in dir.rglob("syslog-ng-core*.deb"):
            pkg_timestamp = timestamp_regexp.findall(deb_file.name)[0]
            pkg_timestamps.append(pkg_timestamp)
        pkg_timestamps.sort()

        return pkg_timestamps

    def __remove_pkgs_with_timestamp(self, dir: Path, timestamps_to_remove: List[str]) -> None:
        for timestamp in timestamps_to_remove:
            for deb_file in dir.rglob("*{}*.deb".format(timestamp)):
                self._log_info("Removing old nightly package.", path=str(deb_file.resolve()))
                deb_file.unlink()

    def __remove_old_pkgs(self, indexed_dir: Path) -> None:
        platform_dirs = list(filter(lambda path: path.is_dir(), indexed_dir.glob("*")))

        for platform_dir in platform_dirs:
            pkg_timestamps = self.__get_pkg_timestamps_in_dir(platform_dir)

            if len(pkg_timestamps) <= NightlyDebIndexer.PKGS_TO_KEEP:
                continue

            timestamps_to_remove = pkg_timestamps[: -NightlyDebIndexer.PKGS_TO_KEEP]
            self.__remove_pkgs_with_timestamp(platform_dir, timestamps_to_remove)

    def _prepare_indexed_dir(self, incoming_dir: Path, indexed_dir: Path) -> None:
        super()._prepare_indexed_dir(incoming_dir, indexed_dir)
        self.__remove_old_pkgs(indexed_dir)
