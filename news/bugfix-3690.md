`syslog-format`: fixing the check-hostname(yes|no) option

The check-hostname(yes|no) option detected every value as invalid, causing a parse error when enabled.
