#############################################################################
# Copyright (c) 2023 One Identity LLC.
# Copyright (c) 2022 Balazs Scheidler <bazsi77@gmail.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
# As an additional exemption you are allowed to compile & link against the
# OpenSSL libraries as published by the OpenSSL project. See the file
# COPYING for details.
#
#############################################################################
# pylint: disable=unused-import,function-redefined,useless-parent-delegation

try:
    from _syslogng import LogSource, LogFetcher, LogFetcherResult
    from _syslogng import InstantAckTracker, ConsecutiveAckTracker, BatchedAckTracker

except ImportError:
    import warnings
    from enum import Enum, auto

    warnings.warn("You have imported the syslogng package outside of syslog-ng, "
                  "thus some of the functionality is not available. "
                  "Defining fake classes for those exported by the underlying syslog-ng code")

    # Fake LogSource
    class LogSource(dict):
        def __init__(self):
            pass

        def set_transport_name(self, name):
            pass

    # Fake LogFetcher
    class LogFetcher(dict):
        def __init__(self):
            pass

        def set_transport_name(self, name):
            pass

    class LogFetcherResult(Enum):
        ERROR = auto()
        NOT_CONNECTED = auto()
        SUCCESS = auto()
        TRY_AGAIN = auto()
        NO_DATA = auto()

    # Fake InstantAckTracker
    class InstantAckTracker(dict):
        def __init__(self, ack_callback):
            self.ack_callback = ack_callback

    # Fake ConsecutiveAckTracker
    class ConsecutiveAckTracker(dict):
        def __init__(self, ack_callback):
            self.ack_callback = ack_callback

    # Fake BatchedAckTracker
    class BatchedAckTracker(dict):
        def __init__(self, timeout, batch_size, batched_ack_callback):
            self.timeout = timeout
            self.batch_size = batch_size
            self.batched_ack_callback = batched_ack_callback


class LogSource(LogSource):
    """Base class for building an asynchronous source driver

    A LogSource is a base class that allows the implementation of a
    syslog-ng source driver.  Derive your source implementation from this
    class and override the run() method to add your specific behaviour.

    Internally, this class starts a dedicated thread and allows you to
    use an asynchronous framework such as asyncio to produce messages.

    It easily maps to any source mechanism that pushes messages to syslog-ng
    (e.g.  queueing protocols such as mqtt or HTTP server implementations)
    """
    flags = {}
    parse_options = None

    def __init__(self):
        """Constructs this LogSource instance
        """
        super().__init__()

    def init(self, options):
        """Initialize this LogSource instance

        Returns:
            bool: True to indicate success
        """
        return True

    def deinit(self):
        """Deinitialize this LogSource instance
        """

    def run(self):
        """Run the main loop for the source

        This method is invoked by syslog-ng to start producing messages in
        an asynchronous manner.  This function is executed in a dedicated
        thread and should run until request_exit() is invoked.

        Usually this would either start polling some API or implement the
        server side of a push based protocol, using a framework such as asyncio.

        Whenever a new message is available, just call post_message() with
        the new message.

        Returns:
            None
        """
        raise NotImplementedError

    def post_message(self, msg):
        """Post a message as an output for this source

        post_message() can be called from within run() to post messages to
        syslog-ng, as the output of the source driver being implemented.

        Flow control is implemented in two ways:
          - blocking mode: post_message() would block if flow control kicks
            in (e.g.  there's no room in our output window).  This is simple
            but your main event loop will be suspended until syslog-ng is
            able to accept messages again.

          - non-blocking mode: if suspend() and wakeup() methods are
            implemented in your class, post_message() would not block,
            rather invoke suspend() to indicate that no further messages
            is to be produced and wakeup() when it is possible to produce
            messages again.  This mode allows the continued execution of the
            async main loop even while syslog-ng is unable to accept
            messages.

        Arguments:
            msg: LogMessage
                the log message to be posted
        """
        super().post_message(msg)

    def close_batch(self):
        """Close the current source side batch

        Source side batching helps syslog-ng to amortize expensive work
        across a larger chunk of messages, instead of doing it for each
        message.

        An example where this batching is beneficial is when feeding a
        destination queue and instead of taking a lock on the queue for
        every message (causing contention), we only take it once per batch.

        The native drivers built into syslog-ng will typically close batches
        once every mainloop iteration, allowing a single iteration to
        process multiple messages.  For instance, if we are receiving
        multiple messages in a single TCP datagram, all of those messages
        will be processed as a part of the same batch.

        In Python based log sources, a batch will automatically be closed
        after every message posted via `post_message()`, except if
        `self.auto_close_batches` is set to `False` during initialization.

        In case `self.auto_close_batches` is set to False, the driver has to
        call `close_batch()` explicitly, preferably at a natural boundary
        between incoming batches of messages.  A good example is when we
        retrieve several messages via the same HTTP REST call, then the
        right time to close the batch would be after the last message in the
        response is posted.
        """
        super().close_batch()

    def set_transport_name(self, name):
        """Set the transport name used to retrieve messages

        This function can be called to customize the $TRANSPORT name-value pair.
        """
        super().set_transport_name(name)

    def request_exit(self):
        """Request the main loop to exit

        This function is invoked by syslog-ng from its main thread when this
        source should stop executing.  The expected result of this function
        is that its run() method returns to its caller.
        """
        raise NotImplementedError


class LogFetcher(LogFetcher):
    """Base class for building synchronous (blocking) source driver

    A LogFetcher is a base class that allows the implementation of a
    syslog-ng source driver.  This class starts a dedicated thread and
    regularly executes the fetch() method in that thread to fetch the next
    message to be emitted as the output of the log source.

    It easily maps to any source mechanism that you need to poll regularly
    for new messages (e.g.  HTTP based log access APIs).
    """
    ERROR = FETCH_ERROR = LogFetcherResult.ERROR
    NOT_CONNECTED = FETCH_NOT_CONNECTED = LogFetcherResult.NOT_CONNECTED
    SUCCESS = FETCH_SUCCESS = LogFetcherResult.SUCCESS
    TRY_AGAIN = FETCH_TRY_AGAIN = LogFetcherResult.TRY_AGAIN
    NO_DATA = FETCH_NO_DATA = LogFetcherResult.NO_DATA

    flags = {}
    parse_options = None

    def __init__(self):
        """Constructs this LogFetcher instance
        """
        super().__init__()

    def init(self, options):
        """Initialize this LogFetcher instance

        Returns:
            bool: True to indicate success
        """
        return True

    def deinit(self):
        """Deinitialize this LogFetcher instance
        """

    def open(self):
        """Open the connection to the target service

        This function is called by syslog-ng to open a connection to the
        target service.  It should return True to indicate success or False
        otherwise.  If the connection to the target service fails, syslog-ng
        will try to reconnect until it succeeds, retrying every
        time_reopen() seconds.
        """
        return True

    def close(self):
        """Close the connection to the target service"""

    def fetch(self):
        """Function to fetch the next message

        This function is executed from the thread associated with LogFetcher
        every time syslog-ng is able to consume a new message.

        Returns:
            (result, message): (LogFetcherResult, LogMessage)
                the function needs to return a 2-tuple, which comprises of a
                status code and a message instance.  Possible status codes
                are defined as members in the LogFetcher class or as values
                in the LogFetchrResult enum.
        """
        raise NotImplementedError

    def request_exit(self):
        """Request fetch() to exit

        This function is invoked by syslog-ng from its main thread when this
        source should stop executing.
        The method is optional, use it when fetch() has blocking operations and
        it has to be interrupted when shutting down syslog-ng.
        """

    def set_transport_name(self, name):
        """Set the transport name used to retrieve messages

        This function can be called to customize the $TRANSPORT name-value pair.
        """
        super().set_transport_name(name)


class InstantAckTracker(InstantAckTracker):
    def __init__(self, ack_callback):
        super().__init__(ack_callback=ack_callback)


class ConsecutiveAckTracker(ConsecutiveAckTracker):
    def __init__(self, ack_callback):
        super().__init__(ack_callback=ack_callback)


class BatchedAckTracker(BatchedAckTracker):
    def __init__(self, timeout, batch_size, batched_ack_callback):
        super().__init__(timeout=timeout, batch_size=batch_size, batched_ack_callback=batched_ack_callback)
