`syslog()` source driver: add support for RFC6587 style auto-detection of
octet-count based framing to avoid confusion that stems from the sender
using a different protocol to the server.  This is automatically enabled for
both the `transport(tcp)` and the `transport(tls)` settings.  You can disable
auto-detection by using `transport(text)` or `transport(framed)`.
