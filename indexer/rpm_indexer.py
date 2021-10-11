from .indexer import Indexer


class RpmIndexer(Indexer):
    def __init__(self) -> None:
        raise NotImplementedError()


class ReleaseRpmIndexer(RpmIndexer):
    def __init__(self) -> None:
        raise NotImplementedError()


class NightlyRpmIndexer(RpmIndexer):
    def __init__(self) -> None:
        raise NotImplementedError()
