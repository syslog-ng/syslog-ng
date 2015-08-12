class LogDestination(object):

    def init(self):
        """This method is called at initialization time"""
        return True

    def open(self):
        """Open a connection to the target service.
        Mind that this function is called after every time-reopen() if
        the destination is suspended."""
        return True

    def is_opened(self):
        """Check if the connection to the target is able to receive messages
        Mind that this function is called before every `send(self, msg)`!"""
        return True

    def send(self, msg):
        """Send a message to the target service

        It should return True to indicate success, False will suspend the
        destination for a period specified by the time-reopen() option."""
        pass

    def close(self):
        """Close the connection to the target service.
        Mint that this function is called if the destination is idle
        for longer than the time specified in time-reap()."""
        pass

    def deinit(self):
        """This method is called at deinitialization time"""
        pass

class DummyPythonDest(LogDestination):

    def send(self, msg):
        print('queue', msg)
        return True
