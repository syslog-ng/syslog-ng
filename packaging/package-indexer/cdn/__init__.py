from inspect import isclass
from typing import Dict, Type

from .cdn import CDN
from .azure_cdn import AzureCDN

__all__ = [
    "CDN",
    "AzureCDN",
    "get_implementations",
]


def get_implementations() -> Dict[str, Type[CDN]]:
    implementations = {}

    for cls_name in __all__:
        cls = globals()[cls_name]
        if isclass(cls) and cls != CDN and issubclass(cls, CDN):
            implementations[cls.get_config_keyword()] = cls

    return implementations
