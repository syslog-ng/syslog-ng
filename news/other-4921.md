`syslog-ng-ctl`: do not show orphan metrics for `stats prometheus`

As the `stats prometheus` command is intended to be used to forward metrics
to Prometheus or any other time-series database, displaying orphaned metrics
should be avoided in order not to insert new data points when a given metric
is no longer alive.

In case you are interested in the last known value of orphaned counters, use
the `stats` or `query` subcommands.
