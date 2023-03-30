#!/usr/bin/env python3
#############################################################################
# Copyright (c) 2020 One Identity
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
from urllib.request import urlopen
from urllib.error import HTTPError
from json import loads
from argparse import ArgumentParser


def get_last_issue_or_pr_id():
    ISSUES_API = "https://api.github.com/repos/syslog-ng/syslog-ng/issues?state=all&sort=created&direction=desc"

    raw_response = urlopen(ISSUES_API).read().decode("utf-8")
    json_response = loads(raw_response)
    issues = list(json_response)

    return int(issues[0]["number"])


def is_a_discussion(id):
    DISCUSSIONS_URL = "https://github.com/syslog-ng/syslog-ng/discussions/"

    try:
        urlopen(DISCUSSIONS_URL + str(id))
        return True
    except HTTPError as e:
        if e.getcode() == 404:
            return False

    return False


def get_next_pr_id():
    last_issue_or_pr_id = get_last_issue_or_pr_id()

    for i in range(1, 100):
        id = last_issue_or_pr_id + i
        if not is_a_discussion(id):
            return id

    return -1


def parse_arguments():
    parser = ArgumentParser()
    parser.add_argument("-r", "--raw", action="store_true",
                        help="Only prints the next PR ID. Suitable for scripts.")

    return parser.parse_args()


def main():
    args = parse_arguments()
    next_pr_id = get_next_pr_id()

    if (next_pr_id == -1):
        print("Failed to calculate next PR ID")
        return

    if args.raw:
        print(next_pr_id)
    else:
        print("The next PR ID will be {}.".format(next_pr_id))


if __name__ == "__main__":
    main()
