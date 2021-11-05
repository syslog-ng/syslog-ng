#!/bin/env python3
import logging
from argparse import ArgumentParser, _ArgumentGroup, _SubParsersAction
from pathlib import Path
from typing import List

from azure.identity import ClientSecretCredential

from indexer import Indexer, NightlyDebIndexer, ReleaseDebIndexer


def add_common_required_arguments(required_argument_group: _ArgumentGroup) -> None:
    required_argument_group.add_argument(
        "--full-permission-container-connection-string",
        type=str,
        required=True,
        help=(
            'The "Connection string", generated in the "Shared access signature" menu of the "Storage account".'
            "This token must have full permissions, including permission to delete."
        ),
    )
    required_argument_group.add_argument(
        "--cdn-client-id",
        type=str,
        required=True,
        help='The "Application (client) ID" of the "App", that has permissions to the "Content Delivery Network Endpoint".',
    )
    required_argument_group.add_argument(
        "--cdn-tenant-id",
        type=str,
        required=True,
        help='The "Directory (tenant) ID" of the "App", that has permissions to the "Content Delivery Network Endpoint".',
    )
    required_argument_group.add_argument(
        "--cdn-client-secret",
        type=str,
        required=True,
        help='The "Client secret" of the "App", that has permissions to the "Content Delivery Network Endpoint".',
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
    required_argument_group.add_argument(
        "--gpg-key-path",
        type=str,
        required=True,
        help="The path of the GPG key, used to sign the indexed packages.",
    )
    required_argument_group.add_argument(
        "--no-delete-permission-container-connection-string",
        type=str,
        required=True,
        help=(
            'The "Connection string", generated in the "Shared access signature" menu of the "Storage account".'
            "This token must not have permission to delete."
        ),
    )


def parse_args() -> dict:
    parser = ArgumentParser()
    subparsers = parser.add_subparsers(metavar="suite", dest="suite", required=True)

    prepare_nightly_subparser(subparsers)
    prepare_stable_subparser(subparsers)

    return vars(parser.parse_args())


def init_logging(args: dict) -> None:
    handlers = []

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


def main() -> None:
    args = parse_args()
    init_logging(args)

    cdn_credential = ClientSecretCredential(
        tenant_id=args["cdn_tenant_id"],
        client_id=args["cdn_client_id"],
        client_secret=args["cdn_client_secret"],
    )

    indexers: List[Indexer] = []

    if args["suite"] == "nightly":
        indexers.append(
            NightlyDebIndexer(
                full_permission_container_connection_string=args["full_permission_container_connection_string"],
                cdn_credential=cdn_credential,
            )
        )
    elif args["suite"] == "stable":
        indexers.append(
            ReleaseDebIndexer(
                full_permission_container_connection_string=args["full_permission_container_connection_string"],
                no_delete_permission_container_connection_string=args[
                    "no_delete_permission_container_connection_string"
                ],
                cdn_credential=cdn_credential,
                run_id=args["run_id"],
                gpg_key_path=Path(args["gpg_key_path"]),
            )
        )
    else:
        raise NotImplementedError("Unexpected suite: {}".format(args["suite"]))

    for indexer in indexers:
        indexer.index()


if __name__ == "__main__":
    main()
