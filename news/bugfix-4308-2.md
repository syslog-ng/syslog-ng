`disk-buffer`: Fixed disk-queue file becoming corrupt when changing `disk-buf-size()`.

`syslog-ng` now continues with the originally set `disk-buf-size()`.
Note that changing the `disk-buf-size()` of an existing disk-queue was never supported,
but could cause errors, which are fixed now.
