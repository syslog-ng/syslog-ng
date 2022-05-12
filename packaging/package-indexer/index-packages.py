#!/bin/env python3
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
from argparse import ArgumentParser, _ArgumentGroup, _SubParsersAction
from pathlib import Path
from sys import stdin
from typing import List

from indexer import Indexer, NightlyDebIndexer, ReleaseDebIndexer
from config import Config


def add_common_required_arguments(required_argument_group: _ArgumentGroup) -> None:
    mutually_exclusive_group = required_argument_group.add_mutually_exclusive_group(required=True)
    mutually_exclusive_group.add_argument(
        "--config-file-path",
        type=str,
        help="The path of the config file in yaml format. Cannot be used with `--config-content`.",
    )
    mutually_exclusive_group.add_argument(
        "--config-content",
        type=str,
        help="The raw content of the config in yaml format. Cannot be used with `--config-file-path`.",
    )


def add_common_optional_arguments(parser: ArgumentParser) -> None:
    parser.add_argument(
        "--gpg-key-passphrase-from-stdin",
        action="store_true",
        help="If this option is set, the passphrase of the GPG-key file will be read from STDIN.",
    )
    parser.add_argument(
        "--log-file",
        type=str,
        help="Also log more verbosely into this file.",
    )


def prepare_nightly_subparser(subparsers: _SubParsersAction) -> None:
    parser = subparsers.add_parser(
        "nightly",
        help="Index nightly packages.",
    )
    add_common_optional_arguments(parser)

    required_argument_group = parser.add_argument_group("required arguments")
    add_common_required_arguments(required_argument_group)


def prepare_stable_subparser(subparsers: _SubParsersAction) -> None:
    parser = subparsers.add_parser(
        "stable",
        help="Index stable packages.",
    )
    add_common_optional_arguments(parser)

    required_argument_group = parser.add_argument_group("required arguments")
    add_common_required_arguments(required_argument_group)

    required_argument_group.add_argument(
        "--run-id",
        type=str,
        required=True,
        help='The "run-id" of the "draft-release" GitHub Actions job.',
    )


def parse_args() -> dict:
    parser = ArgumentParser()
    subparsers = parser.add_subparsers(metavar="suite", dest="suite", required=True)

    prepare_nightly_subparser(subparsers)
    prepare_stable_subparser(subparsers)

    return vars(parser.parse_args())


def init_logging(args: dict) -> None:
    handlers: List[logging.Handler] = []

    stream_handler = logging.StreamHandler()
    stream_handler.setLevel(logging.INFO)
    handlers.append(stream_handler)

    if "log_file" in args.keys() and args["log_file"] is not None:
        file_handler = logging.FileHandler(args["log_file"])
        file_handler.setLevel(logging.DEBUG)
        handlers.append(file_handler)

    logging.basicConfig(
        format="%(asctime)s\t[%(name)s]\t%(message)s",
        handlers=handlers,
    )


def load_config(args: dict) -> Config:
    if "config_file_path" in args.keys() and args["config_file_path"] is not None:
        return Config.from_file(Path(args["config_file_path"]))
    elif "config_content" in args.keys() and args["config_content"] is not None:
        return Config.from_string(args["config_content"])
    else:
        raise KeyError("Missing config related option.")


def construct_indexers(cfg: Config, args: dict) -> List[Indexer]:
    suite = args["suite"]

    incoming_remote_storage_synchronizer = cfg.create_incoming_remote_storage_synchronizer(suite)
    indexed_remote_storage_synchronizer = cfg.create_indexed_remote_storage_synchronizer(suite)
    cdn = cfg.create_cdn(suite)

    gpg_key_passphrase = stdin.read() if args["gpg_key_passphrase_from_stdin"] else None

    indexers: List[Indexer] = []

    if suite == "nightly":
        indexers.append(
            NightlyDebIndexer(
                incoming_remote_storage_synchronizer=incoming_remote_storage_synchronizer,
                indexed_remote_storage_synchronizer=indexed_remote_storage_synchronizer,
                cdn=cdn,
            )
        )
    elif suite == "stable":
        indexers.append(
            ReleaseDebIndexer(
                incoming_remote_storage_synchronizer=incoming_remote_storage_synchronizer,
                indexed_remote_storage_synchronizer=indexed_remote_storage_synchronizer,
                cdn=cdn,
                run_id=args["run_id"],
                gpg_key_path=Path(cfg.get_gpg_key_path()),
                gpg_key_passphrase=gpg_key_passphrase,
            )
        )
    else:
        raise NotImplementedError("Unexpected suite: {}".format(suite))

    return indexers


def main() -> None:
    args = parse_args()
    init_logging(args)

    cfg = load_config(args)

    indexers = construct_indexers(cfg, args)
    for indexer in indexers:
        indexer.index()


if __name__ == "__main__":
    main()
