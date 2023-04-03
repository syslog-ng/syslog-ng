The following Prometheus metrics have been renamed:

`log_path_{in,e}gress` -> `route_{in,e}gress_total`
`internal_source` -> `internal_events_total`

The `internal_queue_length` stats counter has been removed.
It was deprecated since syslog-ng 3.29.
