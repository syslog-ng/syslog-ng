`transport`: Fixed a PROXY protocol v2 length check that could abort syslog-ng when the
header length exactly matched the staging buffer size. The request is now rejected cleanly
instead of tripping the internal assertion.
