`syslog-ng-ctl`: Formatting the output of the `syslog-ng-ctl stats` and `syslog-ng-ctl query` commands is unified.

Both commands got a new `--format` (`-m`) argument that can control the output format of the given stat or query. The following formats are supported:

- `kv` - the legacy key-value-pairs e.g. `center.queued.processed=0` (only for the `query` command yet)
- `csv` - comma separated values e.g. `center;;queued;a;processed;0`
- `prometheus` - the prometheus scraper ready format e.g. `syslogng_center_processed{stat_instance="queued"} 0`