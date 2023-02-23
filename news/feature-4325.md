`stats`: New metric system (Prometheus data format)

A new `syslog-ng-ctl stats prometheus` command has been introduced, which can
be used to query syslog-ng metrics in a format that conforms the Prometheus
text-based exposition format.

`syslog-ng-ctl stats prometheus --with-legacy-metrics` displays legacy metrics
as well.
Legacy metrics do not follow Prometheus' metric and label conventions.

This feature is experimental; the output of `stats prometheus`
(names, labels, etc.) may change in the next 2-3 releases.
