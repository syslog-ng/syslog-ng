`network(), syslog()`: Fixed a potential crash for TLS destinations during reload

In case of a TLS connection, if the handshake didn't happen before reloading syslog-ng,
it crashed on the first message sent to that destination.