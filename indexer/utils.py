import logging
from pathlib import Path
from subprocess import call
from typing import Dict, List, TextIO, Optional

logger = logging.getLogger("utils")
logger.setLevel(logging.INFO)


def execute_command(
    command: List[str],
    dir: Optional[Path] = None,
    env: Optional[Dict[str, str]] = None,
    stdout: Optional[TextIO] = None,
) -> None:
    log_extras = {k: str(v) for k, v in filter(lambda kv: kv[1] is not None, locals().items())}
    logger.info("Executing command.\t{}".format(log_extras))

    rc = call(
        command,
        stdout=stdout,
        cwd=dir,
        env=env,
    )
    if rc != 0:
        raise ChildProcessError("`{}` failed. dir={} env={} rc={}.".format(" ".join(command), dir, env, rc))


def move_file_without_overwrite(src: Path, dst: Path) -> None:
    if dst.exists():
        raise FileExistsError("Refusing to move file, destination already exists. src={} dst={}".format(src, dst))
    src.rename(dst)
