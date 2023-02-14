`kubernetes` source: Added `key-delimiter()` option.

Some metadata fields can contain `.`-s in their name. This does not work with syslog-ng-s macros, which
by default use `.` as a delimiter. The added `key-delimiter()` option changes this behavior by storing
the parsed metadata fields with a custom delimiter. In order to reach the fields, the accessor side has
to use the new delimiter format, e.g. `--key-delimiter` option in `$(format-json)`.
