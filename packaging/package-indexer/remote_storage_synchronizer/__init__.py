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

from inspect import isclass
from typing import Dict, Type

from .remote_storage_synchronizer import RemoteStorageSynchronizer
from .azure_container_synchronizer import AzureContainerSynchronizer

__all__ = [
    "RemoteStorageSynchronizer",
    "AzureContainerSynchronizer",
    "get_implementations",
]


def get_implementations() -> Dict[str, Type[RemoteStorageSynchronizer]]:
    implementations = {}

    for cls_name in __all__:
        cls = globals()[cls_name]
        if isclass(cls) and cls != RemoteStorageSynchronizer and issubclass(cls, RemoteStorageSynchronizer):
            implementations[cls.get_config_keyword()] = cls

    return implementations
