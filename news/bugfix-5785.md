`network-source`, `syslog-source`: Fixed a reload leaving already-open TCP connections on the
stale wire protocol when switching between `network()` and `syslog()` (or any other
transport/logproto change) on the same source.

Kept-alive connections are now compared against the new configuration's protocol and
transport name; a mismatching connection is discarded on reload instead of being silently
reused with the old framing, and its list node is now properly freed instead of leaked.
