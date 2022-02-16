`transport(text-with-nuls)`: a new transport mechanism was added for
the `network()` driver that allows NUL characters within the message. NOTE:
syslog-ng does not support embedded NUL characters everywhere, so it is
recommended that you also use `flags(no-multi-line)` that causes NUL
characters to be replaced by space.
