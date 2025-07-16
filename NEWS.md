4.9.0
=====

## Highlights

  * `stats-exporter`: Added two new sources, `stats-exporter()` and `stats-exporter-dont-log()`, which directly serve the output of `syslog-ng-ctl stats` and `syslog-ng-ctl query` to a http  scraper. The only difference is that `stats-exporter-dont-log()` suppresses log messages from incoming scraper requests, ensuring no messages appear in the log path. Meanwhile, `stats-exporter()` logs unparsed messages, storing incoming scraper HTTP requests in the `MSG` field.

    Example usage for a Prometheus Scraper which logs the HTTP request of the scraper to /var/log/scraper.log:

    ``` config
    @version: 4.9
    @include "scl.conf"

    source s_prometheus_stat {
        stats-exporter(
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
    `scrape-pattern(string)` – the pattern used to match the HTTP header of incoming scraping requests. A stat response will be generated and sent only if the header matches the pattern string
    `scrape-freq-limit(non-negative-int)` - limits the frequency of repeated scraper requests to the specified number of seconds. Any repeated request within this period will be ignored. A value of 0 means no limit
    `single-instance(yes/no)` - if set to `yes` only one scraper connection and request will be allowed at once
    ([#5259](https://github.com/syslog-ng/syslog-ng/pull/5259))

  * `syslog()` source driver: add support for RFC6587 style auto-detection of
    octet-count based framing to avoid confusion that stems from the sender
    using a different protocol to the server.  This behaviour can be enabled
    by using `transport(auto)` option for the `syslog()` source.
    ([#5322](https://github.com/syslog-ng/syslog-ng/pull/5322))

  * `wildcard-file`: Added inotify-based regular file change detection using the existing inotify-based directory monitor.

    This improves efficiency on OSes like Linux, where only polling was available before, significantly reducing CPU usage while enhancing change detection accuracy.

    To enable this feature, inotify kernel support is required, along with `monitor-method()` set to `inotify` or `auto`, and `follow-freq()` set to 0.
    ([#5315](https://github.com/syslog-ng/syslog-ng/pull/5315))

## Features

  * `cisco`: Added support for Cisco Nexus NXOS 9.3 syslog format.

    The parser now recognises NXOS 9.3 timestamps in `YYYY MMM DD HH:MM:SS` format and handles the different
    sequence number prefix (`: ` instead of `seqno: `) used by NXOS 9.3 compared to traditional IOS formats.

    Example Cisco configuration:

    - NXOS: `(config)# logging server <syslog-ng-server-ip> port 2000`
    - IOS: `(config)# logging host <syslog-ng-server-ip> transport udp port 2000`

    Example syslog-ng configuration:

    ```
    @include "scl.conf"

    source s_cisco {
        network(ip(0.0.0.0) transport("udp") port(2000) flags(no-parse));
    };

    parser p_cisco {
        cisco-parser();
    };

    destination d_placeholder {
        # Define your destination here
    };

    log {
        source(s_cisco);
        parser(p_cisco);
        destination(d_placeholder);
    };
    ```
    ([#5412](https://github.com/syslog-ng/syslog-ng/pull/5412))

  * bigquery(), google-pubsub-grpc(): Added service-account() authentication option.

    Example usage:
    ```
    destination {
        google-pubsub-grpc(
            project("test")
            topic("test")
            auth(service-account(key ("path_to_service_account_key.json")))
        );
    };
    ```

    Note: In contrary to the `http()` destination's similar option,
    we do not need to manually set the audience here as it is
    automatically recognized by the underlying gRPC API.
    ([#5270](https://github.com/syslog-ng/syslog-ng/pull/5270))

  * gRPC based destinations: Added `response-action()` option

    With this option, it is possible to fine tune how syslog-ng
    behaves in case of different gRPC results.

    Supported by the following destination drivers:
      * `opentelemetry()`
      * `loki()`
      * `bigquery()`
      * `clickhouse()`
      * `google-pubsub-grpc()`

    Supported gRPC results:
      * ok
      * unavailable
      * cancelled
      * deadline-exceeded
      * aborted
      * out-of-range
      * data-loss
      * unknown
      * invalid-argument
      * not-found
      * already-exists
      * permission-denied
      * unauthenticated
      * failed-precondition
      * unimplemented
      * internal
      * resource-exhausted

    Supported actions:
      * disconnect
      * drop
      * retry
      * success

    Usage:
    ```
    google-pubsub-grpc(
      project("my-project")
      topic("my-topic")
      response-action(
        not-found => disconnect
        unavailable => drop
      )
    );
    ```
    ([#5332](https://github.com/syslog-ng/syslog-ng/pull/5332))

  * `s3`: Added two new options

    * `content-type()`: users now can change the content type of the objects uploaded by syslog-ng.
    * `use_checksum()`: This option allows the users to change the default checksum settings for
    S3 compatible solutions that don't support checksums. Requires botocore 1.36 or above. Acceptable values are
    `when_supported` (default) and `when_required`.

    Example:
    ```
    s3(
    	url("http://localhost:9000")
    	bucket("testbucket")
    	object_key("testobject")
    	access_key("<ACCESS_KEY_ID>")
    	secret_key("<SECRET_ACCESS_KEY>")
    	content_type("text/plain")
    	use_checksum("when_required")
    );
    ```
    ([#5286](https://github.com/syslog-ng/syslog-ng/pull/5286))

  * `loki()`: Added `batch-bytes()` and `compression()` options.
    ([#5174](https://github.com/syslog-ng/syslog-ng/pull/5174))

  * `syslog-ng-ctl`: Formatting the output of the `syslog-ng-ctl stats` and `syslog-ng-ctl query` commands is unified.

    Both commands got a new `--format` (`-m`) argument that can control the output format of the given stat or query. The following formats are supported:

    - `kv` - the legacy key-value-pairs e.g. `center.queued.processed=0` (only for the `query` command yet)
    - `csv` - comma separated values e.g. `center;;queued;a;processed;0`
    - `prometheus` - the prometheus scraper ready format e.g. `syslogng_center_processed{stat_instance="queued"} 0`
    ([#5248](https://github.com/syslog-ng/syslog-ng/pull/5248))

  * `network()`, `syslog()` sources: add `$PEERIP` and `$PEERPORT` macros

    The `$PEERIP` and `$PEERPORT` macros always display the address and port of the direct sender.
    In most cases, these values are identical to `$SOURCEIP` and `$SOURCEPORT`.
    However, when dealing with proxied protocols, `$PEERIP` and `$PEERPORT` reflect the proxy's address and port,
    while `$SOURCEIP` and `$SOURCEPORT` indicate the original source of the message.
    ([#5291](https://github.com/syslog-ng/syslog-ng/pull/5291))

  * `webhook()`,`opentelemetry()` sources: support `input_event_bytes` metrics
    ([#5324](https://github.com/syslog-ng/syslog-ng/pull/5324))

  * `freebsd-audit()`: added a simple source SCL to collect FreeBSD audit logs using the built-in praudit program

    https://www.syslog-ng.com/community/b/blog/posts/freebsd-audit-source-for-syslog-ng
    ([#5383](https://github.com/syslog-ng/syslog-ng/pull/5383))

  * `webhook()`: headers support

    `include-request-headers(yes)` stores request headers under the `${webhook.headers}` key, allowing further processing

    `proxy-header("x-forwarded-for")` helps retain the sender's original IP and the proxy's IP address

    (`$SOURCEIP`, `$PEERIP`).
    ([#5333](https://github.com/syslog-ng/syslog-ng/pull/5333))

  * `check-program`: Introduced as a flag for global or source options.

    By default, this flag is set to false. Enabling the check-program flag triggers `program` name validation for `RFC3164` messages. Valid `program` names must adhere to the following criteria:

    Contain only these characters: `[a-zA-Z0-9-_/().]`
    Include at least one alphabetical character.
    If a `program` name fails validation, it will be considered part of the log message.

    Example:

    ```
    source { network(flags(check-hostname, check-program)); };
    ```
    ([#5264](https://github.com/syslog-ng/syslog-ng/pull/5264))

  * `syslog(transport(proxied-*))` and `network(transport(proxied-*))`: changed
    where HAProxy transport saved the original source and destination addresses.
    Instead of using dedicated `PROXIED_*` name-value pairs, use the usual
    `$SOURCEIP`, `$SOURCEPORT`, `$DESTIP` and `$DESTPORT` macros, making haproxy
    based connections just like native ones.

    `$SOURCEPORT`: added new macro which expands to the source port of the peer.
    ([#5305](https://github.com/syslog-ng/syslog-ng/pull/5305))

  * `opentelemetry()`, `syslog-ng-otlp()`: Added `keep-alive()` options.

    Keepalive can be configured with the `time()`, `timeout()`
    and `max-pings-without-data()` options of the `keep-alive()` block.

    ``` config
    opentelemetry(
        ...
        keep-alive(time(20000) timeout(10000) max-pings-without-data(0))
    );
    ```
    ([#5174](https://github.com/syslog-ng/syslog-ng/pull/5174))

  * `bigquery()`: Added `auth()` options.

    Similarly to other gRPC based destination drivers, the `bigquery()`
    destination now accepts different authentication methods, like
    `adc()`, `alts()`, `insecure()` and `tls()`.

    ``` config
    bigquery (
        ...
        auth(
            tls(
                ca-file("/path/to/ca.pem")
                key-file("/path/to/key.pem")
                cert-file("/path/to/cert.pem")
            )
        )
    );
    ```
    ([#5174](https://github.com/syslog-ng/syslog-ng/pull/5174))

  * `cloud-auth`: Added `azure-monitor()` destination

    Added oauth2 authentication for azure monitor destinations.

    Example usage:

    ``` config
    azure-monitor(
         dcr-id("dcr id")
         dce-uri("dce uri")
         stream_name("stream name")
         auth(
              tenant-id("tenant id")
              app-id("app id")
              app-secret("app secret")
         )
    )
    ```
    ([#5293](https://github.com/syslog-ng/syslog-ng/pull/5293))

  * `multi-line-mode()`: Added a new mutiline detection mode `empty-line-separated` that, as its name suggests, reads and treats all messages as one till it receives an empty line (which contains only a `\r`, `\n` or `\r\n` sequence).
    ([#5259](https://github.com/syslog-ng/syslog-ng/pull/5259))

  * `google-pubsub-grpc()`: Added a new destination that sends logs to Google Pub/Sub via the gRPC interface.

    Example config:
    ```
    google-pubsub-grpc(
      project("my_project")
      topic($topic)

      data($MESSAGE)
      attributes(
        timestamp => $S_ISODATE,
        host => $HOST,
      )

      workers(4)
      batch-timeout(1000) # ms
      batch-lines(1000)
    );
    ```

    The `project()` and `topic()` options are templatable.
    The default service endpoint can be changed with the `service_endpoint()` option.
    ([#5266](https://github.com/syslog-ng/syslog-ng/pull/5266))

  * `ivykis`: We have switched to [our own fork](https://github.com/balabit/ivykis) of ivykis as the source for builds when using syslog-ng’s internal ivykis option (`--with-ivykis=internal` in autotools or `-DIVYKIS_SOURCE=internal` in CMake).

    We recommend switching to this internal version, as it includes new features not available in the [original version](https://github.com/buytenh/ivykis) and likely never will be.
    ([#5307](https://github.com/syslog-ng/syslog-ng/pull/5307))

  * `ivykis`: Fixed and merged the in development phase `io_uring` based polling method solution to [our ivykis fork](https://github.com/balabit/ivykis).

    This is am experimental integration and not selected by default, you must activate it directly either using the `IV_EXCLUDE_POLL_METHOD` or `IV_SELECT_POLL_METHOD` as described [here](https://syslog-ng.github.io/admin-guide/060_Sources/020_File/001_File_following).
    ([#5312](https://github.com/syslog-ng/syslog-ng/pull/5312))

  * `file()`, `wildcard-file()`: Added `follow-method()` option.

    |Accepted values:| legacy \| inotify \| poll \| system |

    This option controls how syslog-ng will follow file changes.\
    The default `legacy` mode preserves the pre-4.9 version file follow-mode behavior of syslog-ng, which is based on the value of follow-freq().\
    The `poll` value forces syslog-ng to poll for file changes at the interval specified by the monitor-freq() option, even if a more efficient method (such as `inotify` or `kqueue`) is available.\
    If `inotify` is selected and supported by the platform, syslog-ng uses it to detect changes in source files. This is the most efficient and least resource-consuming option available on Linux for regular files.\
    The `system` value will use system poll methods (via ivykis) like `port-timer` `port` `dev_poll` `epoll-timerfd` `epoll` `kqueue` `ppoll` `poll` and `uring`. For more information about how to control the system polling methods used, see [How content changes are followed in file() and wildcard-file() sources](https://syslog-ng.github.io/admin-guide/060_Sources/020_File/001_File_following).
    ([#5338](https://github.com/syslog-ng/syslog-ng/pull/5338))

  * `opentelemetry()`, `loki()` destination: Add support for templated `header()` values
    ([#5184](https://github.com/syslog-ng/syslog-ng/pull/5184))

## Bugfixes

  * `syslog-ng-otlp()` destination: Fixed a crash.
    ([#5267](https://github.com/syslog-ng/syslog-ng/pull/5267))

  * Fixed some time parsing and time formatting issues.
    ([#5386](https://github.com/syslog-ng/syslog-ng/pull/5386))

  * syslogformat: Fix integer overflow on set pri
    ([#5254](https://github.com/syslog-ng/syslog-ng/pull/5254))

  * `network(), syslog()`: Fixed a potential crash for TLS destinations during reload

    In case of a TLS connection, if the handshake didn't happen before reloading syslog-ng,
    it crashed on the first message sent to that destination.
    ([#5303](https://github.com/syslog-ng/syslog-ng/pull/5303))

  * `collectd()`: fix not reading server responses
    ([#5390](https://github.com/syslog-ng/syslog-ng/pull/5390))

  * metrics: `syslog-ng-ctl --reset` will no longer reset Prometheus metrics
    ([#5261](https://github.com/syslog-ng/syslog-ng/pull/5261))

  * `rate-limit()`: fix precision issue that could occur at a very low message rate
    ([#5346](https://github.com/syslog-ng/syslog-ng/pull/5346))

  * `network()`, `syslog()` sources and destinations: fix TCP/TLS shutdown
    ([#5271](https://github.com/syslog-ng/syslog-ng/pull/5271))

  * `http`: Fixed a batching related bug that happened with templated URLs and a single worker.
    ([#5281](https://github.com/syslog-ng/syslog-ng/pull/5281))

  * `network()`, `syslog()` destinations: handle async TLS messages (KeyUpdate, etc.)
    ([#5390](https://github.com/syslog-ng/syslog-ng/pull/5390))

## Notes to developers

  * editorconfig: configure supported editors for the project's style
    ([#5331](https://github.com/syslog-ng/syslog-ng/pull/5331))


## Other changes

  * java-modules: Upgrade java `common` and `hdfs` dependencies.
    ([#5366](https://github.com/syslog-ng/syslog-ng/pull/5366))

  * java-modules: Remove depricated java destinations: `elasticsearch2`, `kafka-java` and the `java-http`.

    The following destinations can be used instead:

    - `elasticsearch2` - Both [elastic-datastream()](https://syslog-ng.github.io/admin-guide/070_Destinations/035_elasticsearch-datastream/README) or the [elastic-http()](https://syslog-ng.github.io/admin-guide/070_Destinations/030_Elasticsearch-http/README) can be used.
    - `kafka-java` - The C based [kafka-c()](https://syslog-ng.github.io/admin-guide/070_Destinations/100_Kafka-c/README) destination can be used instead. To help with migration check out the [Shifting from Java implementation to C implementation](https://syslog-ng.github.io/admin-guide/070_Destinations/100_Kafka-c/001_Shifting_from_Java_to_C) page.
    - `java-http` - the C based [http()](https://syslog-ng.github.io/admin-guide/070_Destinations/081_http/README) destination can be used.
    ([#5366](https://github.com/syslog-ng/syslog-ng/pull/5366))

## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Alex Becker, Attila Szakacs, Balazs Scheidler, Bálint Horváth,
David Mandelberg, Eli Schwartz, Hofi, Kovács Gergő Ferenc,
László Várady, Peter Czanik (CzP), Petr Vaganov,
Shiraz, Szilard Parrag, Tamas Pal, Tamás Kosztyu, shifter
