import logging


class GlobalRegister(object):
    def __init__(self):
        pass

    def __del__(self):
        pass

    @staticmethod
    def register_item_in_group(key, value, group):
        group[key] = value
        return group

    @staticmethod
    def is_item_registered(key, group):
        return key in group

    @staticmethod
    def get_registered_item(key, group):
        if key in group:
            return group[key]
        else:
            logging.error("Expected item: [%s] not in group: [%s]" % (key, group.keys()))
            raise KeyError
