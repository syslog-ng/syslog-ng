`stats-exporter`: Added two new sources, `stats-exporter()` and `stats-exporter-dont-log()`, which directly serve the output of `syslog-ng-ctl stats` and `syslog-ng-ctl query` to a http  scraper. The only difference is that `stats-exporter-dont-log()` suppresses log messages from incoming scraper requests, ensuring no messages appear in the log path. Meanwhile, `stats-exporter()` logs unparsed messages, storing incoming scraper HTTP requests in the `MSG` field.

Example usage for a Prometheus Scraper which logs the HTTP request of the scraper to /var/log/scraper.log:

``` config
@version: 4.9
@include "scl.conf"

source s_prometheus_stat {
    stats-exporter-dont-log(
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
    destination { file(/var/log/scraper.log); };
};
```

Example usage for a generic HTTP Scraper which sends e.g. the `GET /stats HTTP/1.1` HTTP request to get statistics of syslog-ng, do not want to log or further process the HTTP requests in the log pipe, and needs the response in CSV format:

``` config
@version: 4.9
@include "scl.conf"

source s_scraper_stat {
    stats-exporter-dont-log(
        ip("0.0.0.0")
        port(8080)
        stat-type("stats")
        stat-format("csv")
        scrape-type("pattern-driven")
        scrape-pattern("GET /stats*")
        scrape-freq-limit(30)
        single-instance(yes)
    );
};

log {
    source(s_scraper_stat);
};
```

Note: A destination is not required for this to work; the `stats-exporter()` source will respond to the scraper regardless of whether a destination is present in the log path.

Available options:

`stat-type(string)` - `query` or `stats`, just like for the `syslog-ng-ctl` command line tool, see there for the details
`stat-query(string)` - the query regex string that can be used to filter the output of a `query` type request
`stat-format(string)` - the output format of the given stats request, like the `-m` option of the `syslog-ng-ctl` command line tool
`scrape-type(string)` – specifies the scraper that sends the HTTP scraping request. Currently, only `prometheus` is supported. You can also use `pattern-driven`, in which case the HTTP header sent by the scraper is matched against the value of `scrape-pattern`, and the stats response is sent only if the pattern matches
`scrape-pattern(string)` – the pattern used to match the HTTP header of incoming scraping requests when `scrape-type` is set to `pattern-driven`
`scrape-freq-limit(non-negative-int)` - limits the frequency of repeated scraper requests to the specified number of seconds. Any repeated request within this period will be ignored. A value of 0 means no limit
`single-instance(yes/no)` - if set to `yes` only one scraper connection and request will be allowed at once
