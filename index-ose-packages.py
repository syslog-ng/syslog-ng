#!/bin/env python3
import logging
from argparse import ArgumentParser, _ArgumentGroup, _SubParsersAction
from pathlib import Path
from typing import List

from indexer import Indexer, NightlyDebIndexer, ReleaseDebIndexer
from config import Config


def add_common_required_arguments(required_argument_group: _ArgumentGroup) -> None:
    required_argument_group.add_argument(
        "--config",
        type=str,
        required=True,
        help="The path of the config file in yaml format.",
    )


def add_common_optional_arguments(parser: ArgumentParser) -> None:
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


def construct_indexers(cfg: Config, args: dict) -> List[Indexer]:
    suite = args["suite"]

    incoming_remote_storage_synchronizer = cfg.create_incoming_remote_storage_synchronizer(suite)
    indexed_remote_storage_synchronizer = cfg.create_indexed_remote_storage_synchronizer(suite)
    cdn = cfg.create_cdn(suite)

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
            )
        )
    else:
        raise NotImplementedError("Unexpected suite: {}".format(suite))

    return indexers


def main() -> None:
    args = parse_args()
    init_logging(args)

    cfg = Config(Path(args["config"]))

    indexers = construct_indexers(cfg, args)
    for indexer in indexers:
        indexer.index()


if __name__ == "__main__":
    main()
