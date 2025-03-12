`prometheus-stats`: Added two new sources, `prometheus-stats()` and `prometheus-stats-dont-log()`, which directly serve the output of `syslog-ng-ctl stats` and `syslog-ng-ctl query` to a Prometheus scraper. The only difference is that `prometheus-stats-dont-log()` suppresses log messages from incoming scraper requests, ensuring no messages appear in the log path. Meanwhile, `prometheus-stats()` logs unparsed messages, storing incoming scraper HTTP requests in the `MSG` field.

You can set it up like:

``` config
@version: 4.9
@include "scl.conf"

source s_prometheus_stat {
    prometheus-stats-dont-log(
        ip("0.0.0.0")
        port(8080)
        stat-type("query")
        stat-query("*")
        scrape-freq-limit(30)
        single-instance(yes)
    );
};

log {
    source(s_prometheus_stat);
};
```

Note: A destination is not required for this to work; the `prometheus-stats()` source will respond to the scraper regardless of whether a destination is present in the log path.

Available options:

`stat-type(string)` - `query` or `stats`, just like for the `syslog-ng-ctl` command line tool, see there for the details
`stat-query(string)` - the query regex string that can be used to filter the output of a `query` type request
`scrape-freq-limit(non-negative-int)` - limits the frequency of repeated scraper requests to the specified number of seconds. Any repeated request within this period will be ignored. A value of 0 means no limit
`single-instance(yes/no)` - if set to `yes` only one scraper connection and request will be allowed at once
