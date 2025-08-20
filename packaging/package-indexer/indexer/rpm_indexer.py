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

import re
from pathlib import Path
from typing import List, Optional
import pexpect
import sys

from cdn import CDN
from remote_storage_synchronizer import RemoteStorageSynchronizer

from indexer import Indexer

from . import utils

CURRENT_DIR = Path(__file__).parent.resolve()


class RPMIndexer(Indexer):
    __rpm_gpg_sign_extra_args = " --pinentry-mode loopback --homedir %s "

    __rpmdefines = {
        "_signature": "gpg",
        "_gpgbin": "/usr/bin/gpg",
    }

    __rpmargs: List[str] = []

    def __init__(
        self,
        incoming_remote_storage_synchronizer: RemoteStorageSynchronizer,
        indexed_remote_storage_synchronizer: RemoteStorageSynchronizer,
        incoming_sub_dir: Path,
        dist_dir: Path,
        cdn: CDN,
        gpg_key_path: Path,
        gpg_key_passphrase: Optional[str],
        gpg_key_name: Optional[str],
    ) -> None:
        super().__init__(
            incoming_remote_storage_synchronizer=incoming_remote_storage_synchronizer,
            indexed_remote_storage_synchronizer=indexed_remote_storage_synchronizer,
            incoming_sub_dir=incoming_sub_dir,
            indexed_sub_dir=Path("yum", dist_dir),
            cdn=cdn,
        )
        self.__gpg_key_path = gpg_key_path.expanduser()
        self.__gpg_key_passphrase = gpg_key_passphrase
        self.__gpg_key_name = gpg_key_name
        if gpg_key_name is None:
            self.__gpg_key_name = "syslog-ng@lists.balabit.hu"

        self.__gpg_home = Path.joinpath(Path.home(), ".gnupg")
        self.__gpg_home.mkdir(mode=0o700, exist_ok=True)

        self.__rpmdefines["_gpg_name"] = str(self.__gpg_key_name)
        self.__rpmdefines["__gpg_path"] = str(self.__gpg_home)
        self.__rpmdefines["_gpg_sign_cmd_extra_args"] = self.__rpm_gpg_sign_extra_args % str(self.__gpg_home)
        self.__add_gpg_key_to_chain(str(self.__gpg_home))

        self.__rpmargs = list(map(lambda x: "--define=%s %s" % (x, self.__rpmdefines[x]), self.__rpmdefines.keys()))

    def __move_files_from_incoming_to_indexed(self, incoming_dir: Path, indexed_dir: Path) -> List[Path]:
        new_files = []
        for file in filter(lambda path: path.is_file(), incoming_dir.rglob("*")):
            if not file.name.endswith(".rpm"):
                continue
            relative_path = file.relative_to(incoming_dir)
            platform = relative_path.parent
            file_name = relative_path.name
            # I do not like this either, but this is the only way to determine the arch currently.
            binary_dir = "aarch64" if platform.name.endswith("arm64") else "x86_64"

            if platform.name.endswith("-arm64"):
                platform = Path(platform.name.rstrip("-arm64"))

            new_path = Path(indexed_dir, platform, binary_dir, file_name)

            self._log_info("Moving file.", src_path=str(file), dst_path=str(new_path))

            new_path.parent.mkdir(parents=True, exist_ok=True)
            utils.move_file_without_overwrite(file, new_path)
            new_files.append(new_path)
        return new_files

    def __sign_rpm_file(self, filepath: Path) -> None:
        signerror = ""
        args = self.__rpmargs + [
            "--addsign",
            str(filepath),
        ]
        self._log_info("Signing RPM file %s" % filepath)
        self._log_info("Command: %s" % args)
        signer = pexpect.spawn("/usr/bin/rpmsign", args, encoding="utf-8")
        signer.logfile_read = sys.stderr
        signer.logfile_send = sys.stderr
        try:
            signer.expect(["Enter pass phrase:", "Enter passphrase:"])
            signer.sendline(self.__gpg_key_passphrase)
            i = signer.expect(["Pass phrase is good", "Pass phrase check failed"])
            if i == 1:
                signerror = "password check failed"
        except pexpect.EOF:
            signerror = "EOF read while running RPM signing tool."
        except pexpect.TIMEOUT:
            signerror = "RPM signing tool timed out."

        signout = "".join(signer.readlines())
        if signer.isalive():
            signer.wait()
        ret = signer.exitstatus
        # it's not a fault if GPG agent cached the unlocked key, and no passphrase have been asked ...
        if ret == 0 and signerror == "EOF read while running RPM signing tool.":
            return
        if ret != 0 or signerror != "":
            self._log_info(
                "Failed to sign RPM file `%s`. \nCause: %s\nRPM's output was: %s\n" % (filepath, signerror, signout)
            )
            raise

    def _prepare_indexed_dir(self, incoming_dir: Path, indexed_dir: Path) -> None:
        new_files = self.__move_files_from_incoming_to_indexed(incoming_dir, indexed_dir)
        for new_file in new_files:
            self.__sign_rpm_file(new_file)

    def _index_pkgs(self, indexed_dir: Path) -> None:
        for binary_dir in ["x86_64", "aarch64"]:
            for pkg_dir in list(indexed_dir.rglob(binary_dir)):
                command = ["createrepo_c", str(pkg_dir)]
                self._log_info("Creating YUM repo at %s" % pkg_dir)
                utils.execute_command(command)

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
        env = {"GNUPGHOME": str(gnupghome)}

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

    def _sign_pkgs(self, indexed_dir: Path) -> None:
        #
        pass


class StableRPMIndexer(RPMIndexer):
    def __init__(
        self,
        incoming_remote_storage_synchronizer: RemoteStorageSynchronizer,
        indexed_remote_storage_synchronizer: RemoteStorageSynchronizer,
        run_id: str,
        cdn: CDN,
        gpg_key_path: Path,
        gpg_key_passphrase: Optional[str],
        gpg_key_name: Optional[str],
    ) -> None:
        super().__init__(
            incoming_remote_storage_synchronizer=incoming_remote_storage_synchronizer,
            indexed_remote_storage_synchronizer=indexed_remote_storage_synchronizer,
            incoming_sub_dir=Path("stable", run_id),
            dist_dir=Path("stable"),
            cdn=cdn,
            gpg_key_path=gpg_key_path,
            gpg_key_passphrase=gpg_key_passphrase,
            gpg_key_name=gpg_key_name,
        )


class NightlyRPMIndexer(RPMIndexer):
    PKGS_TO_KEEP = 10

    def __init__(
        self,
        incoming_remote_storage_synchronizer: RemoteStorageSynchronizer,
        indexed_remote_storage_synchronizer: RemoteStorageSynchronizer,
        cdn: CDN,
        run_id: str,
        gpg_key_path: Path,
        gpg_key_passphrase: Optional[str],
        gpg_key_name: Optional[str],
    ) -> None:
        super().__init__(
            incoming_remote_storage_synchronizer=incoming_remote_storage_synchronizer,
            indexed_remote_storage_synchronizer=indexed_remote_storage_synchronizer,
            incoming_sub_dir=Path("nightly", run_id),
            dist_dir=Path("nightly"),
            cdn=cdn,
            gpg_key_path=gpg_key_path,
            gpg_key_passphrase=gpg_key_passphrase,
            gpg_key_name=gpg_key_name,
        )

    def __get_pkg_timestamps_in_dir(self, directory: Path) -> List[str]:
        timestamp_regexp = re.compile(r"\+([^_]+)_")
        pkg_timestamps: List[str] = []

        for rpm_file in directory.rglob("syslog-ng-core*.rpm"):
            pkg_timestamp = timestamp_regexp.findall(rpm_file.name)[0]
            pkg_timestamps.append(pkg_timestamp)
        pkg_timestamps.sort()

        return pkg_timestamps

    def __remove_pkgs_with_timestamp(self, directory: Path, timestamps_to_remove: List[str]) -> None:
        for timestamp in timestamps_to_remove:
            for rpm_file in directory.rglob("*{}*.rpm".format(timestamp)):
                self._log_info("Removing old nightly package.", path=str(rpm_file.resolve()))
                rpm_file.unlink()

    def __remove_old_pkgs(self, indexed_dir: Path) -> None:
        platform_dirs = list(filter(lambda path: path.is_dir(), indexed_dir.glob("*")))

        for platform_dir in platform_dirs:
            pkg_timestamps = self.__get_pkg_timestamps_in_dir(platform_dir)

            if len(pkg_timestamps) <= NightlyRPMIndexer.PKGS_TO_KEEP:
                continue

            timestamps_to_remove = pkg_timestamps[: -NightlyRPMIndexer.PKGS_TO_KEEP]
            self.__remove_pkgs_with_timestamp(platform_dir, timestamps_to_remove)

    def _prepare_indexed_dir(self, incoming_dir: Path, indexed_dir: Path) -> None:
        super()._prepare_indexed_dir(incoming_dir, indexed_dir)
        self.__remove_old_pkgs(indexed_dir)
