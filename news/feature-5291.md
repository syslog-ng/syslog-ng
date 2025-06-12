`network()`, `syslog()` sources: add `$PEERIP` and `$PEERPORT` macros

The `$PEERIP` and `$PEERPORT` macros always display the address and port of the direct sender.
In most cases, these values are identical to `$SOURCEIP` and `$SOURCEPORT`.
However, when dealing with proxied protocols, `$PEERIP` and `$PEERPORT` reflect the proxy's address and port,
while `$SOURCEIP` and `$SOURCEPORT` indicate the original source of the message.
