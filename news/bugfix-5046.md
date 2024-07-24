`syslog-ng-ctl`: fix escaping of `stats prometheus`

Metric labels (for example, the ones produced by `metrics-probe()`) may contain control characters, invalid UTF-8 or `\`
characters. In those specific rare cases, the escaping of the `stats prometheus` output was incorrect.
