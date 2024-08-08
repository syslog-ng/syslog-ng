`metrics-probe()`: fix disappearing metrics from `stats prometheus` output

`metrics-probe()` metrics became orphaned and disappeared from the `syslog-ng-ctl stats prometheus` output
whenever an ivykis worker stopped (after 10 seconds of inactivity).
