`$SEVERITY instead of $LEVEL`: syslog-ng historically choose the term
`level` to refer to the severity of the message that was used in the
template language, filter expressions and so on. RFC3164 however, a few
years later adopted the term `severity` for the same purpose. This change
will add support for `$SEVERITY` macro and the `severity()` filter
expression, while keeping the old ones for compatibility.
