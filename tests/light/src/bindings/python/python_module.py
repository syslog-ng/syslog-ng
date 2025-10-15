#############################################################################
# Copyright (c) 2025 Narkhov Evgeny <evgenynarkhov2@gmail.com>
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
import shutil
import string
from pathlib import Path
from random import choice
from typing import Dict

from src.common.file import copy_file


class DummyPythonModule(object):
    """ Provides paths for dummy python module used in testing. """

    def __init__(self, source_dir: Path, random_postfix_len: int = 10):
        module_name = 'test_%s' % ''.join(choice(string.ascii_letters) for _ in range(random_postfix_len))
        self.module_path = source_dir / module_name
        self.init_file = self.module_path / "__init__.py"
        self.files: Dict[str, Path] = {}

        self.module_path.mkdir()
        self.init_file.touch()

    def add_file(self, file: Path):
        """ Adds file to a module. File will be removed once all python tests are complete. """

        if file.stem in self.files:
            raise FileExistsError("the file %s cannot be copied since it clashes with %s under %s" % (file, file.stem, self.files[file.stem]))

        self.files[file.stem] = file
        copy_file(file, (self.module_path / file.stem).with_suffix(".py"))

    def replace_file(self, file: Path, stem: str):
        """ Similar to add_file(), but overrides existing file in self.files under **stem**. """

        if stem not in self.files:
            raise KeyError

        self.files.pop(stem).unlink()
        self.add_file(file)

    @property
    def module_name(self):
        return self.module_path.stem

    def teardown(self):
        shutil.rmtree(self.module_path)
