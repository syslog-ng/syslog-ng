`metrics`: new metric to monitor destination reachability

`syslogng_output_unreachable` is a bool-like metric, which shows whether a
destination is reachable or not.

`sum()` can be used to count all unreachable outputs, hence the negated name.

It is currently available for the `network()`, `syslog()`, `unix-*()`
destinations, and threaded destinations (`http()`, `opentelemetry()`, `redis()`,
`mongodb()`, `python()`, etc.).
