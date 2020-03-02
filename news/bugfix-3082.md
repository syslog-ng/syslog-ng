`network` sources: Added workaround for a TLS 1.3 bug to prevent data loss

Due to a bug in the OpenSSL TLS 1.3 implementation (openssl/openssl#10880),
it is possible to lose messages when one-way communication protocols are used, -
such as the syslog protocol over TLS ([RFC 5425](https://tools.ietf.org/html/rfc5425),
[RFC 6587](https://tools.ietf.org/html/rfc6587)) - and the connection is closed
by the client right after sending data.

The bug is in the TLS 1.3 session ticket handling logic of OpenSSL.

To prevent such data loss, we've disabled TLS 1.3 session tickets in all
syslog-ng network sources. Tickets are used for session resumption, which is
currently not supported by syslog-ng.

The `loggen` testing tool also received some bugfixes (#3064), which reduce the
likelihood of data loss if the target of loggen has not turned off session
tickets.

If you're sending logs to third-party OpenSSL-based TLS 1.3 collectors, we
recommend turning session tickets off in those applications as well until the
OpenSSL bug is fixed.
