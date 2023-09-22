#############################################################################
# Copyright (c) 2023 Attila Szakacs
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

from __future__ import annotations

from io import SEEK_END
from gzip import GzipFile
from pathlib import Path
from typing import Optional


class CompressableFileBuffer:
    def __init__(
        self,
        path: Path,
        compress: bool = False,
        compresslevel: Optional[int] = None,
        prev_buffer: Optional[CompressableFileBuffer] = None,
    ) -> None:
        self.__finished = False

        self.__path = path.resolve()
        self.__path.parent.mkdir(parents=True, exist_ok=True)
        self.__path.touch()
        self.__buffer = self.__path.open(mode="r+b")
        self.__buffer.seek(0, SEEK_END)

        self.__gzfile: Optional[GzipFile] = None

        if prev_buffer is not None:
            self.__gzfile = prev_buffer.gzipfile
            if self.__gzfile:
                self.__gzfile.fileobj = self.__buffer
        elif compress:
            assert compresslevel is not None
            self.__gzfile = GzipFile(
                filename="",
                fileobj=self.__buffer,
                mode="wb",
                compresslevel=compresslevel,
            )

    @staticmethod
    def create_initial(path: Path, compress: bool, compresslevel: Optional[int] = None) -> CompressableFileBuffer:
        return CompressableFileBuffer(path=path, compress=compress, compresslevel=compresslevel)

    @staticmethod
    def load_finished(path: Path) -> CompressableFileBuffer:
        self = CompressableFileBuffer(path=path)
        self.__finished = True
        return self

    def create_next(self, path: Path) -> CompressableFileBuffer:
        return CompressableFileBuffer(path=path, prev_buffer=self)

    @property
    def path(self) -> Path:
        return self.__path

    @property
    def gzipfile(self) -> Optional[GzipFile]:
        return self.__gzfile

    def write(self, data: bytes) -> int:
        """Raises OSError on failure."""
        assert not self.__finished
        if self.__gzfile:
            return self.__gzfile.write(data)
        return self.__buffer.write(data)

    def flush(self) -> None:
        if self.__gzfile:
            self.__gzfile.flush()
        self.__buffer.flush()

    def is_empty(self) -> int:
        return self.tell() == 0

    def tell(self) -> int:
        return self.__buffer.tell()

    def close(self) -> None:
        if self.__gzfile:
            self.__gzfile.close()
        self.__buffer.close()

    def getvalue(self) -> bytes:
        self.flush()
        self.__buffer.seek(0)
        return self.__buffer.read()

    def finish(self) -> None:
        if self.__finished:
            return

        if self.__gzfile:
            self.__gzfile.close()
            self.__gzfile = None
        self.flush()
        self.__finished = True

    def unlink(self) -> None:
        self.__path.unlink()
