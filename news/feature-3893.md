syslog-format: accept ISO timestamps that incorrectly use a space instead of
a 'T' to delimit the date from the time portion.  For example, a
"2021-01-01T12:12:12" timestamp is well formed according to RFC5424 (which
uses a subset of ISO8601, see https://datatracker.ietf.org/doc/html/rfc5424#section-6.2.3).
Some systems simply use a space instead of a 'T'.  The same format is
accepted for both RFC3164 (e.g.  udp(), tcp() and network() sources) and
RFC5424 (e.g.  syslog() source).
