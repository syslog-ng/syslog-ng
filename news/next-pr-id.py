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
from json import loads
from argparse import ArgumentParser


def get_next_pr_id():
    ISSUES_API = "https://api.github.com/repos/syslog-ng/syslog-ng/issues?state=all&sort=created&direction=desc"

    raw_response = urlopen(ISSUES_API).read().decode("utf-8")
    json_response = loads(raw_response)
    issues = list(json_response)
    last_id = int(issues[0]["number"])

    return last_id + 1


def parse_arguments():
    parser = ArgumentParser()
    parser.add_argument("-r", "--raw", action="store_true",
                        help="Only prints the next PR ID. Suitable for scripts.")

    return parser.parse_args()


def main():
    args = parse_arguments()
    next_pr_id = get_next_pr_id()

    if args.raw:
        print(next_pr_id)
    else:
        print("The next PR ID will be {}.".format(next_pr_id))


if __name__ == "__main__":
    main()
