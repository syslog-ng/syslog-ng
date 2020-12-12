`grouping-by()`: fix deadlock when context expires

In certain cases, the `grouping-by()` parser could get stuck when a message
context expired, causing a deadlock in syslog-ng.

This has been fixed.
