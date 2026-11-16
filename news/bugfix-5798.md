`graylog2()`, `$(format-gelf)`: Fixed sending only a NUL byte for messages with a non-numeric PID.

The `_pid` field was cast with `int()`, which fails on a PID such as `worker-3`, so the whole JSON
object was dropped. `_pid` is now only sent when the PID is plain decimal digits; otherwise it is
left out and the message is sent.
