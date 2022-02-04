from __future__ import annotations

import logging
from abc import ABC, abstractmethod
from pathlib import Path


class CDN(ABC):
    def __init__(self) -> None:
        self.__logger = CDN.__create_logger()

    @staticmethod
    @abstractmethod
    def get_config_keyword() -> str:
        pass

    @staticmethod
    @abstractmethod
    def from_config(config: dict) -> CDN:
        pass

    @abstractmethod
    def refresh_cache(self, path: Path) -> None:
        pass

    @staticmethod
    def __create_logger() -> logging.Logger:
        logger = logging.getLogger("CDN")
        logger.setLevel(logging.DEBUG)
        return logger

    def _prepare_log(self, message: str, **kwargs: str) -> str:
        if len(kwargs) > 0:
            message += "\t{}".format(kwargs)
        return message

    def _log_info(self, message: str, **kwargs: str) -> None:
        log = self._prepare_log(message, **kwargs)
        self.__logger.info(log)

    def _log_debug(self, message: str, **kwargs: str) -> None:
        log = self._prepare_log(message, **kwargs)
        self.__logger.debug(log)
