`syslog-ng-ctl`: new flag for pruning statistics

`syslog-ng-ctl stats --remove-orphans` can be used to remove "orphaned" statistic counters.
It is useful when, for example, a templated file destination (`$YEAR.$MONTH.$DAY`) produces a lot of stats,
and one wants to remove those abandoned counters occasionally/conditionally.
