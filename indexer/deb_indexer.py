from pathlib import Path
from subprocess import call

from .indexer import Indexer, ClientSecretCredential


class DebIndexer(Indexer):
    def __init__(
        self,
        container_connection_string: str,
        incoming_container_sub_dir: Path,
        dist_dir: Path,
        cdn_credential: ClientSecretCredential,
    ) -> None:
        super().__init__(
            container_connection_string=container_connection_string,
            incoming_container_sub_dir=incoming_container_sub_dir,
            indexed_container_sub_dir=Path("apt", "dists", dist_dir),
            cdn_credential=cdn_credential,
        )

    def __move_files_from_incoming_to_indexed(self, incoming_dir: Path, indexed_dir: Path) -> None:
        for file in filter(lambda path: path.is_file(), incoming_dir.rglob("*")):
            relative_path = file.relative_to(incoming_dir)
            platform = relative_path.parent
            file_name = relative_path.name

            new_path = Path(indexed_dir, platform, "binary-amd64", file_name)

            self._log_info("Moving file.", src_path=str(file), dst_path=str(new_path))

            new_path.parent.mkdir(parents=True, exist_ok=True)
            file.rename(new_path)

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
                self._log_info(
                    "Creating `Packages` file.",
                    command=" ".join(command),
                    dir=str(dir),
                    output_file_path=str(packages_file_path),
                )
                status = call(
                    command,
                    stdout=packages_file,
                    cwd=str(dir),
                )
                if status != 0:
                    raise ChildProcessError("`{}` failed in dir: {}. rc={}.".format(" ".join(command), dir, status))

    def __create_release_file(self, indexed_dir: Path) -> None:
        #  TODO: implement
        pass

    def _index_pkgs(self, incoming_dir: Path, indexed_dir: Path) -> None:
        self.__move_files_from_incoming_to_indexed(incoming_dir, indexed_dir)
        self.__create_packages_files(indexed_dir)
        self.__create_release_file(indexed_dir)


class ReleaseDebIndexer(DebIndexer):
    def __init__(
        self,
        container_connection_string: str,
        run_id: str,
        cdn_credential: ClientSecretCredential,
        gpg_key_path: Path,
    ) -> None:
        self.__gpg_key_path = gpg_key_path
        super().__init__(
            container_connection_string=container_connection_string,
            incoming_container_sub_dir=Path("release", run_id),
            dist_dir=Path("stable"),
            cdn_credential=cdn_credential,
        )

    def _sign_pkgs(self, indexed_dir: Path) -> None:
        #  TODO: implement
        pass


class NightlyDebIndexer(DebIndexer):
    def __init__(
        self,
        container_connection_string: str,
        cdn_credential: ClientSecretCredential,
    ) -> None:
        super().__init__(
            container_connection_string=container_connection_string,
            incoming_container_sub_dir=Path("nightly"),
            dist_dir=Path("nightly"),
            cdn_credential=cdn_credential,
        )

    def _sign_pkgs(self, indexed_dir: Path) -> None:
        pass  # We do not sign the nightly package
