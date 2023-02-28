4.1.0
=====

## Highlights

### PROXY protocol v2 support ([#4211](https://github.com/syslog-ng/syslog-ng/pull/4211))

We've added support for PROXY protocol v2 (`transport(proxied-tcp)`), a protocol
used by network load balancers, such as Amazon Elastic Load Balancer and
HAProxy, to carry original source/destination address information, as described
in https://www.haproxy.org/download/1.8/doc/proxy-protocol.txt

### Metrics revised

#### Prometheus metric format ([#4325](https://github.com/syslog-ng/syslog-ng/pull/4325))

A new metric system has been introduced to syslog-ng, where metrics are
identified by names and partitioned by labels, which is similar to the
[Prometheus data model](https://prometheus.io/docs/concepts/data_model/).

The `syslog-ng-ctl stats prometheus` command can be used to query syslog-ng
metrics in a format that conforms to the Prometheus text-based exposition
format.

`syslog-ng-ctl stats prometheus --with-legacy-metrics` displays legacy metrics
as well. Legacy metrics do not follow Prometheus' metric and label conventions.

#### Classification (metadata-based metrics) ([#4318](https://github.com/syslog-ng/syslog-ng/pull/4318))

`metrics-probe()`, a new parser has also been added, which counts messages
passing through based on the metadata of each message. The parser creates
labeled metrics based on the fields of the message.

Both the key and labels can be set in the config, the values of the labels can
be templated. E.g.:
```
parser p_metrics_probe {
  metrics-probe(
    key("custom_key")  # adds "syslogng_" prefix => "syslogng_custom_key"
    labels(
      "custom_label_name_1" => "foobar"
      "custom_label_name_2" => "${.custom.field}"
    )
  );
};
```
With this config, it creates counters like these:
```
syslogng_custom_key{custom_label_name_1="foobar", custom_label_name_2="bar"} 1
syslogng_custom_key{custom_label_name_1="foobar", custom_label_name_2="foo"} 1
syslogng_custom_key{custom_label_name_1="foobar", custom_label_name_2="baz"} 3
```

The minimal config creates counters with the key
`syslogng_classified_events_total` and labels `app`, `host`, `program` and
`source`. E.g.:
```
parser p_metrics_probe {
  metrics-probe();
};
```
With this config, it creates counters like these:
```
syslogng_classified_events_total{app="example-app", host="localhost", program="baz", source="s_local_1"} 3
syslogng_classified_events_total{app="example-app", host="localhost", program="bar", source="s_local_1"} 1
syslogng_classified_events_total{app="example-app", host="localhost", program="foo", source="s_local_1"} 1
```

#### Named log paths (path ingress/egress metrics) ([#4344](https://github.com/syslog-ng/syslog-ng/pull/4344))

It is also possible to create named log paths, for example:

```
log top-level {
    source(s_local);

    log inner-1 {
        filter(f_inner_1);
        destination(d_local_1);
    };

    log inner-2 {
        filter(f_inner_2);
        destination(d_local_2);
    };
};
```

Each named log path counts its ingress and egress messages:
```
syslogng_log_path_ingress{id="top-level"} 114
syslogng_log_path_ingress{id="inner-1"} 114
syslogng_log_path_ingress{id="inner-2"} 114
syslogng_log_path_egress{id="top-level"} 103
syslogng_log_path_egress{id="inner-1"} 62
syslogng_log_path_egress{id="inner-2"} 41
```

Note that the egress statistics only count the messages which have been have not been filtered out from the related
log path, it does care about whether there are any destinations in it or that any destination delivers or drops the
message.

The above three features are experimental; the output of `stats prometheus`
(names, labels, etc.) and the metrics created by `metrics-probe()` and named log
paths may change in the next 2-3 releases.

## Features

  * `$(format-date)`: add a new template function to format time and date values

    `$(format-date [options] format-string [timestamp])`

    `$(format-date)` takes a timestamp in the DATETIME representation and
    formats it according to an strftime() format string.  The DATETIME
    representation in syslog-ng is a UNIX timestamp formatted as a decimal
    number, with an optional fractional part, where the seconds and the
    fraction of seconds are separated by a dot.

    If the timestamp argument is missing, the timestamp of the message is
    used.

    Options:
      `--time-zone <TZstring>` -- override timezone of the original timestamp
    ([#4202](https://github.com/syslog-ng/syslog-ng/pull/4202))

  * `syslog-parser()` and all syslog related sources: accept unquoted RFC5424
    SD-PARAM-VALUEs instead of rejecting them with a parse error.

    `sdata-parser()`: this new parser allows you to parse an RFC5424 style
    structured data string. It can be used to parse this relatively complex
    format separately.
    ([#4281](https://github.com/syslog-ng/syslog-ng/pull/4281))

  * `system()` source: the `system()` source was changed on systemd platforms to
    fetch journal messages that relate to the current boot only (e.g.  similar
    to `journalctl -fb`) and to ignore messages generated in previous boots,
    even if those messages were succesfully stored in the journal and were not
    picked up by syslog-ng.  This change was implemented as the journald access
    APIs work incorrectly if time goes backwards across reboots, which is an
    increasingly frequent event in virtualized environments and on systems that
    lack an RTC.  If you want to retain the old behaviour, please bypass the
    `system()` source and use `systemd-journal()` directly, where this option
    can be customized. The change is not tied to `@version` as we deemed the new
    behaviour fixing an actual bug. For more information consult #2836.

    `systemd-journald()` source: add `match-boot()` and `matches()` options to
    allow you to constrain the collection of journal records to a subset of what
    is in the journal. `match-boot()` is a yes/no value that allows you to fetch
    messages that only relate to the current boot. `matches()` allows you to
    specify one or more filters on journal fields.

    Examples:

    ```
    source s_journal_current_boot_only {
      systemd-source(match-boot(yes));
    };

    source s_journal_systemd_only {
      systemd-source(matches(
        "_COMM" => "systemd"
        )
      );
    };
    ```
    ([#4245](https://github.com/syslog-ng/syslog-ng/pull/4245))

  * `date-parser()`: add `value()` parameter to instruct `date-parser()` to store
    the resulting timestamp in a name-value pair, instead of changing the
    timestamp value of the LogMessage.

    `datetime` type representation: typed values in syslog-ng are represented as
    strings when stored as a part of a log message.  syslog-ng simply remembers
    the type it was stored as.  Whenever the value is used as a specific type in
    a type-aware context where we need the value of the specific type, an
    automatic string parsing takes place.  This parsing happens for instance
    whenever syslog-ng stores a datetime value in MongoDB or when
    `$(format-date)` template function takes a name-value pair as parameter.
    The datetime() type has stored its value as the number of milliseconds since
    the epoch (1970-01-01 00:00:00 GMT).  This has now been enhanced by making
    it possible to store timestamps up to nanosecond resolutions along with an
    optional timezone offset.

    `$(format-date)`: when applied to name-value pairs with the `datetime` type,
    use the timezone offset if one is available.
    ([#4319](https://github.com/syslog-ng/syslog-ng/pull/4319))

  * `stats`: Added `syslog-stats()` global `stats()` group option.

    E.g.:
    ```
    options {
      stats(
        syslog-stats(no);
      );
    };
    ```

    It changes the behavior of counting messages based on different syslog-proto fields,
    like `SEVERITY`, `FACILITY`, `HOST`, etc...

    Possible values are:
      * `yes` => force enable
      * `no` => force disable
      * `auto` => let `stats(level())` decide (old behavior)
    ([#4337](https://github.com/syslog-ng/syslog-ng/pull/4337))

  * `kubernetes` source: Added `key-delimiter()` option.

    Some metadata fields can contain `.`-s in their name. This does not work with syslog-ng-s macros, which
    by default use `.` as a delimiter. The added `key-delimiter()` option changes this behavior by storing
    the parsed metadata fields with a custom delimiter. In order to reach the fields, the accessor side has
    to use the new delimiter format, e.g. `--key-delimiter` option in `$(format-json)`.
    ([#4213](https://github.com/syslog-ng/syslog-ng/pull/4213))

## Bugfixes

  * Fix conditional evaluation with a dangling filter

    We've fixed a bug that caused conditional evaluation (if/else/elif) and certain logpath flags (`final`, `fallback`)
    to occasionally malfunction. The issue only happened in certain logpath constructs; examples can be found in the
    PR description.
    ([#4058](https://github.com/syslog-ng/syslog-ng/pull/4058))
  * `python`: Fixed a bug, where `PYTHONPATH` was ignored with `python3.11`.
    ([#4298](https://github.com/syslog-ng/syslog-ng/pull/4298))
  * `disk-buffer`: Fixed disk-queue file becoming corrupt when changing `disk-buf-size()`.

    `syslog-ng` now continues with the originally set `disk-buf-size()`.
    Note that changing the `disk-buf-size()` of an existing disk-queue was never supported,
    but could cause errors, which are fixed now.
    ([#4308](https://github.com/syslog-ng/syslog-ng/pull/4308))
  * `dqtool`: fix `dqtool assign`
    ([#4355](https://github.com/syslog-ng/syslog-ng/pull/4355))
  * `example-diskq-source`: Fixed failing to read the disk-queue content in some cases.
    ([#4308](https://github.com/syslog-ng/syslog-ng/pull/4308))
  * `default-network-drivers()`: Added support for the `log-iw-size()` option with a default value of 1000.
    Making it possible to adjust the `log-iw-size()` for the TCP/TLS based connections, when changing the `max-connections()` option.
    ([#4328](https://github.com/syslog-ng/syslog-ng/pull/4328))
  * `apache-accesslog-parser()`: fix rawrequest escaping binary characters
    ([#4303](https://github.com/syslog-ng/syslog-ng/pull/4303))
  * `dqtool`: Fixed `dqtool cat` failing to read the content in some cases.
    ([#4308](https://github.com/syslog-ng/syslog-ng/pull/4308))
  * Fixed a rare main loop related crash on FreeBSD.
    ([#4262](https://github.com/syslog-ng/syslog-ng/pull/4262))
  * Fix a warning message that was displayed incorrectly:
    "The actual number of worker threads exceeds the number of threads estimated at startup."
    ([#4282](https://github.com/syslog-ng/syslog-ng/pull/4282))
  * Fix minor memory leak related to tznames
    ([#4334](https://github.com/syslog-ng/syslog-ng/pull/4334))

## Packaging

  * `dbparser`: libdbparser.so has been renamed to libcorrelation.so.
    ([#4294](https://github.com/syslog-ng/syslog-ng/pull/4294))
  * `systemd-journal`: Fixed a linker error, which occurred, when building with `--with-systemd-journal=optional`.
    ([#4304](https://github.com/syslog-ng/syslog-ng/pull/4304))
    ([#4302](https://github.com/syslog-ng/syslog-ng/pull/4302))

## Notes to developers

  * `LogThreadedSourceDriver` and `Fetcher`: implement source-side batching
    support on the input path by assigning a thread_id to dynamically spawned
    input threads (e.g. those spawned by LogThreadedSourceDriver) too.  To
    actually improve performance the source driver should disable automatic
    closing of batches by setting `auto_close_batches` to FALSE and calling
    log_threaded_source_close_batch() explicitly.
    ([#3969](https://github.com/syslog-ng/syslog-ng/pull/3969))

## Other changes

  * stats related options: The stats related options have been groupped to a new `stats()` block.

    This affects the following global options:
      * `stats-freq()`
      * `stats-level()`
      * `stats-lifetime()`
      * `stats-max-dynamics()`

    These options have been kept for backward compatibility, but they have been deprecated.

    Migrating from the old stats options to the new ones looks like this.
    ```
    @version: 4.0

    options {
        stats-freq(1);
        stats-level(1);
        stats-lifetime(1000);
        stats-max-dynamics(10000);
    };
    ```
    ```
    @version: 4.1

    options {
        stats(
            freq(1)
            level(1)
            lifetime(1000)
            max-dynamics(10000)
        );
    };
    ```

    **Breaking change**
    For more than a decade `stats()` was a deprecated alias to `stats-freq()`, now it is used as the name
    of the new block. If you have been using `stats(xy)`, use `stats(freq(xy))` instead.
    ([#4337](https://github.com/syslog-ng/syslog-ng/pull/4337))
  * `kubernetes` source: Improved error logging, when the pod was unreachable through the python API.
    ([#4305](https://github.com/syslog-ng/syslog-ng/pull/4305))
  * APT repository: Added .gz, .xz and .bz2 compression to the Packages file.
    ([#4313](https://github.com/syslog-ng/syslog-ng/pull/4313))

## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Attila Szakacs, Balazs Scheidler, Bálint Horváth, Gergo Ferenc Kovacs,
Hofi, László Várady, Ronny Meeus, Szilard Parrag
