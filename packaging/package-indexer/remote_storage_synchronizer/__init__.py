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
