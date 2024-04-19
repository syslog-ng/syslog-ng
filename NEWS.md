4.7.1
=====

*This is the combination of the news entries of `4.7.0` and `4.7.1`.*
*`4.7.1` hotfixed two crashes related to configuration reload.*

Read Axoflow's [blog post](https://axoflow.com/axosyslog-release-4-7/) for more details.
You can read more about the new features in the AxoSyslog [documentation](https://axoflow.com/docs/axosyslog-core/).

## Highlights

### Collecting Jellyfin logs

The new `jellyfin()` source, reads Jellyfin logs from its log file output.

Example minimal config:
```
source s_jellyfin {
  jellyfin(
    base-dir("/path/to/my/jellyfin/root/log/dir")
    filename-pattern("log_*.log")
  );
};
```

For more details about Jellyfin logging, see:
  * https://jellyfin.org/docs/general/administration/configuration/#main-configuration
  * https://jellyfin.org/docs/general/administration/configuration/#log-directory

As the `jellyfin()` source is based on a `wildcard-file()` source, all of the
`wildcard-file()` source options are applicable, too.
([#4802](https://github.com/syslog-ng/syslog-ng/pull/4802))

### Collecting *arr logs

Use the newly added `*arr()` sources to read various *arr logs:
  * `lidarr()`
  * `prowlarr()`
  * `radarr()`
  * `readarr()`
  * `sonarr()`
  * `whisparr()`

Example minimal config:
```
source s_radarr {
  radarr(
    dir("/path/to/my/radarr/log/dir")
  );
};
```

The logging module is stored in the `<prefix><module>` name-value pair,
for example: `.radarr.module` => `ImportListSyncService`.
The prefix can be modified with the `prefix()` option.
([#4803](https://github.com/syslog-ng/syslog-ng/pull/4803))

## Features

  * `opentelemetry()`, `syslog-ng-otlp()` source: Added `concurrent-requests()` option.

    This option configures the maximal number of in-flight gRPC requests per worker.
    Setting this value to the range of 10s or 100s is recommended when there are a
    high number of clients sending simultaneously.

    Ideally, `workers() * concurrent-requests()` should be greater or equal to
    the number of clients, but this can increase the memory usage.
    ([#4827](https://github.com/syslog-ng/syslog-ng/pull/4827))

  * `loki()`: Support multi-tenancy with the new `tenant-id()` option
    ([#4812](https://github.com/syslog-ng/syslog-ng/pull/4812))

  * `s3()`: Added support for authentication from environment.

    The `access-key()` and `secret-key()` options are now optional,
    which makes it possible to use authentication methods originated
    from the environment, e.g. `AWS_...` environment variables or
    credentials files from the `~/.aws/` directory.

    For more info, see:
    https://boto3.amazonaws.com/v1/documentation/api/latest/guide/credentials.html
    ([#4881](https://github.com/syslog-ng/syslog-ng/pull/4881))

  * gRPC based drivers: Added `channel-args()` option.

    Affected drivers are:
     * `bigquery()` destination
     * `loki()` destination
     * `opentelemetry()` source and destination
     * `syslog-ng-otlp()` source and destination

    The `channel-args()` option accepts name-value pairs and sets channel arguments
    defined in https://grpc.github.io/grpc/core/group__grpc__arg__keys.html

    Example config:
    ```
      opentelemetry(
        channel-args(
          "grpc.loadreporting" => 1
          "grpc.minimal_stack" => 0
        )
      );
    ```
    ([#4827](https://github.com/syslog-ng/syslog-ng/pull/4827))

  * `${TRANSPORT}` macro: Added support for locally created logs.

    New values are:
     * "local+unix-stream"
     * "local+unix-dgram"
     * "local+file"
     * "local+pipe"
     * "local+program"
     * "local+devkmsg"
     * "local+journal"
     * "local+afstreams"
     * "local+openbsd"
    ([#4777](https://github.com/syslog-ng/syslog-ng/pull/4777))

  * `tags`: Added new built-in tags that help identifying parse errors.

    New tags are:
     * "message.utf8_sanitized"
     * "message.parse_error"
     * "syslog.missing_pri"
     * "syslog.missing_timestamp"
     * "syslog.invalid_hostname"
     * "syslog.unexpected_framing"
     * "syslog.rfc3164_missing_header"
     * "syslog.rfc5424_unquoted_sdata_value"
    ([#4804](https://github.com/syslog-ng/syslog-ng/pull/4804))

  * `mqtt()` source: Added `${MQTT_TOPIC}` name-value pair.

    It is useful for the cases where `topic()` contains wildcards.

    Example config:
    ```
    log {
      source { mqtt(topic("#")); };
      destination { stdout(template("${MQTT_TOPIC} - ${MESSAGE}\n")); };
    };
    ```
    ([#4824](https://github.com/syslog-ng/syslog-ng/pull/4824))

  * `template()`: Added a new template function: `$(tags-head)`

    This template function accepts multiple tag names, and returns the
    first one that is set.

    Example config:
    ```
    # resolves to "bar" if "bar" tag is set, but "foo" is not
    template("$(tags-head foo bar baz)")
    ```
    ([#4804](https://github.com/syslog-ng/syslog-ng/pull/4804))

  * `s3()`: Use default AWS URL if `url()` is not set.
    ([#4813](https://github.com/syslog-ng/syslog-ng/pull/4813))

  * `opentelemetry()`, `syslog-ng-otlp()` source: Added `log-fetch-limit()` option.

    This option can be used to fine tune the performance. To minimize locking while
    moving messages between source and destination side queues, syslog-ng can move
    messages in batches. The `log-fetch-limit()` option sets the maximal size of
    the batch moved by a worker. By default it is equal to `log-iw-size() / workers()`.
    ([#4827](https://github.com/syslog-ng/syslog-ng/pull/4827))

  * `dqtool`: add option for truncating (compacting) abandoned disk-buffers
    ([#4875](https://github.com/syslog-ng/syslog-ng/pull/4875))

## Bugfixes

  * `opentelemetry()`: fix crash when an invalid configuration needs to be reverted
    ([#4910](https://github.com/syslog-ng/syslog-ng/pull/4910))

  * gRPC drivers: fixed a crash when gRPC drivers were used and syslog-ng was reloaded
    ([#4909](https://github.com/syslog-ng/syslog-ng/pull/4909))

  * `opentelemetry()`, `syslog-ng-otlp()` source: Fixed a crash.

    It occurred with multiple `workers()` during high load.
    ([#4827](https://github.com/syslog-ng/syslog-ng/pull/4827))

  * `rename()`: Fixed a bug, which always converted the renamed NV pair to string type.
    ([#4847](https://github.com/syslog-ng/syslog-ng/pull/4847))

  * With IPv6 disabled, there were linking errors
    ([#4880](https://github.com/syslog-ng/syslog-ng/pull/4880))

## Metrics

  * `http()`: Added a new counter for HTTP requests.

    It is activated on `stats(level(1));`.

    Example metrics:
    ```
    syslogng_output_http_requests_total{url="http://localhost:8888/bar",response_code="200",driver="http",id="#anon-destination0#0"} 16
    syslogng_output_http_requests_total{url="http://localhost:8888/bar",response_code="401",driver="http",id="#anon-destination0#0"} 2
    syslogng_output_http_requests_total{url="http://localhost:8888/bar",response_code="502",driver="http",id="#anon-destination0#0"} 1
    syslogng_output_http_requests_total{url="http://localhost:8888/foo",response_code="200",driver="http",id="#anon-destination0#0"} 24
    ```
    ([#4805](https://github.com/syslog-ng/syslog-ng/pull/4805))

  * gRPC based destination drivers: Added gRPC request related metrics.

    Affected drivers:
      * `opentelemetry()`
      * `syslog-ng-otlp()`
      * `bigquery()`
      * `loki()`

    Example metrics:
    ```
    syslogng_output_grpc_requests_total{driver="syslog-ng-otlp",url="localhost:12345",response_code="ok"} 49
    syslogng_output_grpc_requests_total{driver="syslog-ng-otlp",url="localhost:12345",response_code="unavailable"} 11
    ```
    ([#4811](https://github.com/syslog-ng/syslog-ng/pull/4811))

  * New metric to monitor destination reachability

    `syslogng_output_unreachable` is a bool-like metric, which shows whether a
    destination is reachable or not.

    `sum()` can be used to count all unreachable outputs, hence the negated name.

    It is currently available for the `network()`, `syslog()`, `unix-*()`
    destinations, and threaded destinations (`http()`, `opentelemetry()`, `redis()`,
    `mongodb()`, `python()`, etc.).
    ([#4876](https://github.com/syslog-ng/syslog-ng/pull/4876))

  * destinations: Added "syslogng_output_event_retries_total" counter.

    This counter is available for the following destination drivers:
     * `amqp()`
     * `bigquery()`
     * `http()` and all http based drivers
     * `java()`
     * `kafka()`
     * `loki()`
     * `mongodb()`
     * `mqtt()`
     * `opentelemetry()`
     * `python()` and all python based drivers
     * `redis()`
     * `riemann()`
     * `smtp()`
     * `snmp()`
     * `sql()`
     * `stomp()`
     * `syslog-ng-otlp()`

    Example metrics:
    ```
    syslogng_output_event_retries_total{driver="http",url="http://localhost:8888/${path}",id="#anon-destination0#0"} 5
    ```
    ([#4807](https://github.com/syslog-ng/syslog-ng/pull/4807))

  * `syslogng_memory_queue_capacity`

    Shows the capacity (maximum possible size) of each queue.
    Note that this metric publishes `log-fifo-size()`, which only limits non-flow-controlled messages.
    Messages coming from flow-controlled paths are not limited by `log-fifo-size()`, their corresponding
    source `log-iw-size()` is the upper limit.
    ([#4831](https://github.com/syslog-ng/syslog-ng/pull/4831))

## Other changes

  * `opentelemetry()`, `syslog-ng-otlp()` source: Changed the backpressure behavior.

    syslog-ng no longer returns `UNAVAILABLE` to the gRPC request, when it cannot forward
    the received message because of backpressure. Instead, syslog-ng will block until the
    destination can accept more messages.
    ([#4827](https://github.com/syslog-ng/syslog-ng/pull/4827))

  * `opentelemetry()`, `syslog-ng-otlp()` source: `log-iw-size()` is now split between workers.
    ([#4827](https://github.com/syslog-ng/syslog-ng/pull/4827))

  * APT packages: Dropped Debian Buster support.

    Old packages are still available, but new syslog-ng versions will not
    be available on Debian Buster
    ([#4840](https://github.com/syslog-ng/syslog-ng/pull/4840))

  * `dbld`: AlmaLinux 8 support
    ([#4902](https://github.com/syslog-ng/syslog-ng/pull/4902))


## syslog-ng Discord

For a bit more interactive discussion, join our Discord server:

[![Axoflow Discord Server](https://discordapp.com/api/guilds/1082023686028148877/widget.png?style=banner2)](https://discord.gg/E65kP9aZGm)

## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Arpad Kunszt, Attila Szakacs, Balazs Scheidler, Bálint Horváth, Hofi,
Kovács, Gergő Ferenc, László Várady, Peter Marko, shifter
