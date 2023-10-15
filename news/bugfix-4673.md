Fix `$PROTO` value for `transport(tls)` connections, previously it was set
to "0" while in reality these are tcp connections (e.g. "6").

Fix how syslog-ng sets $HOST for V4-mapped addresses in case of IPv6 source
drivers (e.g. udp6()/tcp6() or when using ip-protocol(6) for tcp()/udp()).
Previously V4-mapped addresses would be represented as
"::ffff:<ipv4 address>". This is not wrong per-se, but would potentially
cause the same host to be represented in multiple ways. With the fix,
syslog-ng would just use "<ipv4 address>" in these cases.
