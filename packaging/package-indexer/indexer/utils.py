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
from pathlib import Path
from subprocess import run
from typing import Dict, List, TextIO, Optional

logger = logging.getLogger("utils")
logger.setLevel(logging.INFO)


def execute_command(
    command: List[str],
    dir: Optional[Path] = None,
    env: Optional[Dict[str, str]] = None,
    input: Optional[str] = None,
    stdout: Optional[TextIO] = None,
) -> None:
    log_extras = {k: str(v) for k, v in filter(lambda kv: kv[1] is not None and kv[0] != "input", locals().items())}
    logger.info("Executing command.\t{}".format(log_extras))

    rc = run(
        command,
        input=bytes(input, "utf-8") if input is not None else None,
        stdout=stdout,
        cwd=dir,
        env=env,
    ).returncode
    if rc != 0:
        raise ChildProcessError("`{}` failed. dir={} env={} rc={}.".format(" ".join(command), dir, env, rc))


def move_file_without_overwrite(src: Path, dst: Path) -> None:
    if dst.exists():
        raise FileExistsError("Refusing to move file, destination already exists. src={} dst={}".format(src, dst))
    src.rename(dst)
