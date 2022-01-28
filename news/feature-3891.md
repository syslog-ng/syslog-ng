`flags(no-rfc3164-fallback)`: we added a new flag to sources that parse
incoming syslog data and operate in RFC5424 mode (e.g. syslog-protocol is
also set). With the new flag the automatic fallback to RFC3164 format
is disabled. In this case if the parsing in RFC5424 fails, the
syslog parser would result in an error message. In the case of
syslog-parser(drop-invalid(yes)), the message would be dropped.
