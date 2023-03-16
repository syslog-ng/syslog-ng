Health metrics and `syslog-ng-ctl healthcheck`

A new `syslog-ng-ctl` command has been introduced, which can be used to query a healthcheck status from syslog-ng.
Currently, only 2 basic health values are reported.

`syslog-ng-ctl healthcheck --timeout <seconds>` can be specified to use it as a boolean healthy/unhealthy check.

Health checks are also published as periodically updated metrics.
The frequency of these checks can be configured with the `stats(healthcheck-freq())` option.
The default is 5 minutes.
