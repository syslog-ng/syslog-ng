`syslogformat`: Fixed a heap over-read when parsing digit-only syslog input.

This prevents an out-of-bounds read in the framing check for malformed
octet-counted messages.
