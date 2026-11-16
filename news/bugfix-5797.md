`graylog2()`, `$(format-gelf)`: Fixed sending only a NUL byte when `frac-digits()` is set.

The GELF `timestamp` field was cast with `int64()`, which fails on a fractional `${R_UNIXTIME}`, so
the whole JSON object was dropped and only the terminating NUL byte was sent to Graylog.
The timestamp is now emitted as a JSON number as-is, keeping sub-second precision as GELF allows.
