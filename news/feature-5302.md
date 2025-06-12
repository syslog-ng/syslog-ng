`syslog()` source driver: add support for RFC6587 style auto-detection of
octet-count based framing to avoid confusion that stems from the sender
using a different protocol to the server.  This behaviour can be enabled
by using `transport(auto)` option for the `syslog()` source.
