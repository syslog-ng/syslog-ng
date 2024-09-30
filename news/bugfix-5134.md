`file()`, `stdout()`: fix log sources getting stuck

Due to an acknowledgment bug in the `file()` and `stdout()` destinations,
sources routed to those destinations may have gotten stuck as they were
flow-controlled incorrectly.

This issue occured only in extremely rare cases with regular files, but it
occured frequently with `/dev/stderr` and other slow pseudo-devices.
