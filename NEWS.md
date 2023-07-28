4.3.1
=====

_This is the combination of the news entries of 4.3.0 and 4.3.1. 4.3.1 hotfixed_
_a `python-parser()` related crash and a metrics related memory leak. It also_
_added Ubuntu 23.04 and Debian 12 support for APT packages and the `opensearch()`_
_destination._

Read Axoflow's [blog post](https://axoflow.com/axosyslog-release-4-3/) for more details.

## Highlights

#### `parallelize()` support for pipelines

syslog-ng has traditionally performed processing of log messages arriving
from a single connection sequentially.  This was done to ensure message ordering
as well as most efficient use of CPU on a per message basis.  This mode of
operation is performing well as long as we have a relatively large number
of parallel connections, in which case syslog-ng would use all the CPU cores
available in the system.

In case only a small number of connections deliver a large number of
messages, this behaviour may become a bottleneck.

With the new parallelization feature, syslog-ng gained the ability to
re-partition a stream of incoming messages into a set of partitions, each of
which is to be processed by multiple threads in parallel.  This does away
with ordering guarantees and adds an extra per-message overhead. In exchange
it will be able to scale the incoming load to all CPUs in the system, even
if coming from a single, chatty sender.

To enable this mode of execution, use the new parallelize() element in your
log path:

```
log {
  source {
    tcp(
      port(2000)
      log-iw-size(10M) max-connections(10) log-fetch-limit(100000)
    );
  };
  parallelize(partitions(4));

  # from this part on, messages are processed in parallel even if
  # messages are originally coming from a single connection

  parser { ... };
  destination { ... };
};
```

The config above will take all messages emitted by the tcp() source and push
the work to 4 parallel threads of execution, regardless of how many
connections were in use to deliver the stream of messages to the tcp()
driver.

parallelize() uses round-robin to allocate messages to partitions by default.
You can however retain ordering for a subset of messages with the
partition-key() option.

You can use partition-key() to specify a message template. Messages that
expand to the same value are guaranteed to be mapped to the same partition.

For example:

```
log {
  source {
    tcp(
      port(2000)
      log-iw-size(10M) max-connections(10) log-fetch-limit(100000)
    );
  };
  parallelize(partitions(4) partition-key("$HOST"));

  # from this part on, messages are processed in parallel if their
  # $HOST value differs. Messages with the same $HOST will be mapped
  # to the same partition and are processed sequentially.

  parser { ... };
  destination { ... };
};
```

NOTE: parallelize() requires a patched version of libivykis that contains
this PR https://github.com/buytenh/ivykis/pull/25.  syslog-ng source
releases bundle this version of ivykis in their source trees, so if you are
building from source, be sure to use the internal version
(--with-ivykis=internal).  You can also use Axoflow's cloud native container
image for syslog-ng, named AxoSyslog
(https://github.com/axoflow/axosyslog-docker) which also incorporates this
change.

([#3966](https://github.com/syslog-ng/syslog-ng/pull/3966))

#### Receiving and sending OpenTelemetry (OTLP) messages

The `opentelemetry()` source, parser and destination are now available to receive, parse and send **OTLP/gRPC**
messages.

syslog-ng accepts [logs](https://github.com/open-telemetry/opentelemetry-proto/blob/v0.20.0/opentelemetry/proto/logs/v1/logs.proto), [metrics](https://github.com/open-telemetry/opentelemetry-proto/blob/v0.20.0/opentelemetry/proto/metrics/v1/metrics.proto) and [traces](https://github.com/open-telemetry/opentelemetry-proto/blob/v0.20.0/opentelemetry/proto/trace/v1/trace.proto).

The incoming fields are not available through syslog-ng log message name-value pairs for the user by default.
This is useful for forwarding functionality (the `opentelemetry()` destination can access and format them).
If such functionality is required, you can configure the `opentelemetry()` parser, which maps all the fields
with some limitations.

The behavior of the `opentelemetry()` parser is the following:

The name-value pairs always start with `.otel.` prefix. The type of the message is stored in `.otel.type`
(possible values: `log`, `metric` and `span`). The `resource` info is mapped to `.otel.resource.<...>`
(e.g.: `.otel.resource.dropped_attributes_count`, `.otel.resource.schema_url` ...), the `scope` info 
is mapped to `.otel.scope.<...>` (e.g.: `.otel.scope.name`, `.otel.scope.schema_url`, ...).

The fields of log records are mapped to `.otel.log.<...>` (e.g. `.otel.log.body`, ` .otel.log.severity_text`, ...).

The fields of metrics are mapped to `.otel.metric.<...>` (e.g. `.otel.metric.name`, `.otel.metric.unit`, ...),
the type of the metric is mapped to `.otel.metric.data.type` (possible values: `gauge`, `sum`, `histogram`,
`exponential_histogram`, `summary`) with the actual data mapped to `.otel.metric.data.<type>.<...>`
(e.g.: `.otel.metric.data.gauge.data_points.0.time_unix_nano`, ...).

The fields of traces are mapped to `.otel.span.<...>` (e.g. `.otel.span.name`, `.otel.span.trace_state`, ...).

`repeated` fields are given an index (e.g. `.otel.span.events.5.time_unix_nano`).

The mapping of [`AnyValue`](https://github.com/open-telemetry/opentelemetry-proto/blob/v0.20.0/opentelemetry/proto/common/v1/common.proto#L28) type fields is limited.
`string`, `bool`, `int64`, `double` and `bytes` values are mapped with the respective syslog-ng name-value type
(e.g. `.otel.resource.attributes.string_key` => `string_value`), however `ArrayValue` and `KeyValueList` types
are stored serialized with `protobuf` type. `protobuf` and `bytes` types are not directly available for the
user, unless an explicit type cast is added (e.g. `"bytes(${.otel.log.span_id})"`) or `--include-bytes` is passed
to name-value iterating template functions (e.g. `$(format-json .otel.* --include-bytes)`, which will base64
encode the bytes content).

Three authentication methods are available in the source `auth()` block: `insecure()` (default), `tls()` and `alts()`.
`tls()` accepts the `key-file()`, `cert-file()`, `ca-file()` and `peer-verify()` (possible values:
`required-trusted`, `required-untrusted`, `optional-trusted` and `optional-untrusted`) options.
[ALTS](https://grpc.io/docs/languages/cpp/alts/) is a simple to use authentication, only available within Google's infrastructure.

The same methods are available in the destination `auth()` block, with two differences: `tls(peer-verify())`
is not available, and there is a fourth method, called [ADC](https://cloud.google.com/docs/authentication/application-default-credentials), which accepts the `target-service-account()`
option, where a list of service accounts can be configured to match against when authenticating the server.

Example configs:
```
log otel_forward_mode_alts {
  source {
    opentelemetry(
      port(12345)
      auth(alts())
    );
  };

  destination {
    opentelemetry(
      url("my-otel-server:12345")
      auth(alts())
    );
  };
};

log otel_to_non_otel_insecure {
  source {
    opentelemetry(
      port(12345)
    );
  };

  parser {
    opentelemetry();
  };

  destination {
    network(
      "my-network-server"
      port(12345)
      template("$(format-json .otel.* --shift-levels 1 --include-bytes)\n")
    );
  };
};

log non_otel_to_otel_tls {
  source {
    network(
      port(12346)
    );
  };

  destination {
    opentelemetry(
      url("my-otel-server:12346")
      auth(
        tls(
          ca-file("/path/to/ca.pem")
          key-file("/path/to/key.pem")
          cert-file("/path/to/cert.pem")
        )
      )
    );
  };
};
```

([#4523](https://github.com/syslog-ng/syslog-ng/pull/4523))
([#4510](https://github.com/syslog-ng/syslog-ng/pull/4510))

#### Sending messages to CrowdStrike Falcon LogScale (Humio)

The `logscale()` destination feeds LogScale via the [Ingest API](https://library.humio.com/falcon-logscale/api-ingest.html#api-ingest-structured-data).

Minimal config:
```
destination d_logscale {
  logscale(
    token("my-token")
  );
};
```
Additional options include:
  * `url()`
  * `rawstring()`
  * `timestamp()`
  * `timezone()`
  * `attributes()`
  * `extra-headers()`
  * `content-type()`

([#4472](https://github.com/syslog-ng/syslog-ng/pull/4472))

## Features

  * `afmongodb`: Bulk MongoDB insert is added via the following options

    - `bulk`  (**yes**/no)  turns on/off [bulk insert ](http://mongoc.org/libmongoc/current/bulk.html)usage, `no` forces the old behavior (each log is inserted one by one into the MongoDB)
    - `bulk_unordered` (yes/**no**)  turns on/off [unordered MongoDB bulk operations](http://mongoc.org/libmongoc/current/bulk.html#unordered-bulk-write-operations)
    - `bulk_bypass_validation`  (yes/**no**)  turns on/off [MongoDB bulk operations validation](http://mongoc.org/libmongoc/1.23.3/bulk.html#bulk-operation-bypassing-document-validation)
    - `write_concern` (unacked/**acked**/majority/n > 0)  sets [write concern mode of the MongoDB operations](http://mongoc.org/libmongoc/1.23.3/bulk.html#bulk-operation-write-concerns), both bulk and single

    NOTE: Bulk sending is only efficient if the used collection is constant (e.g. not using templates) or the used template does not lead to too many collections switching within a reasonable time range.
    ([#4483](https://github.com/syslog-ng/syslog-ng/pull/4483))

  * `sql`: Added 2 new options

    - `quote_char` to aid custom quoting for table and index names (e.g. MySQL needs sometimes this for certain identifiers)
    **NOTE**: Using a back-tick character needs a special formatting as syslog-ng uses it for configuration parameter names, so for that use:     `quote_char("``")`  (double back-tick)
    - `dbi_driver_dir` to define an optional DBI driver location for DBD initialization

    NOTE: libdbi and libdbi-drivers OSE forks are updated, `afsql` now should work nicely both on ARM and X86 macOS systems too (tested on macOS 13.3.1 and 12.6.4)

    Please do not use the pre-built ones (e.g. 0.9.0 from Homebrew), build from the **master** of the following
    - https://github.com/balabit-deps/libdbi-drivers
    - https://github.com/balabit-deps/libdbi

    ([#4460](https://github.com/syslog-ng/syslog-ng/pull/4460))

  * `opensearch`: Added a new destination.

    It is similar to `elasticsearch-http()`, with the difference that it does not have the `type()`
    option, which is deprecated and advised not to use.
    ([#4560](https://github.com/syslog-ng/syslog-ng/pull/4560))

## Bugfixes

  * `network()`,`syslog()`,`tcp()` destination: fix TCP keepalive

    `tcp-keepalive-*()` options were broken on the destination side since v3.34.1.
    ([#4559](https://github.com/syslog-ng/syslog-ng/pull/4559))

  * Fixed a hang, which happend when syslog-ng received exremely low CPU time.
    ([#4524](https://github.com/syslog-ng/syslog-ng/pull/4524))

  * `$(format-json)`: Fixed a bug where sometimes an unnecessary comma was added in case of a type cast failure.
    ([#4477](https://github.com/syslog-ng/syslog-ng/pull/4477))

  * Fix flow-control when `fetch-limit()` is set higher than 64K

    In high-performance use cases, users may configure log-iw-size() and
    fetch-limit() to be higher than 2^16, which caused flow-control issues,
    such as messages stuck in the queue forever or log sources not receiving
    messages.
    ([#4528](https://github.com/syslog-ng/syslog-ng/pull/4528))

  * `int32()` and `int64()` type casts: accept hex numbers as proper
    number representations just as the @NUMBER@ parser within db-parser().
    Supporting octal numbers were considered and then rejected as the canonical
    octal representation for numbers in C would be ambigious: a zero padded
    decimal number could be erroneously considered octal. I find that log
    messages contain zero padded decimals more often than octals.
    ([#4535](https://github.com/syslog-ng/syslog-ng/pull/4535))

  * Fixed compilation on platforms where SO_MEMINFO is not available
    ([#4548](https://github.com/syslog-ng/syslog-ng/pull/4548))

  * `python`: `InstantAckTracker`, `ConsecutiveAckTracker` and `BatchedAckTracker` are now called properly.

    Added proper fake classes for the `InstantAckTracker`, `ConsecutiveAckTracker` and `BatchedAckTracker` classes, and the wapper now calls the super class' constructor. 
    Previusly the super class' constructor was not called which caused the python API to never call into the C API, which's result was that that the callback was never called.
    ([#4549](https://github.com/syslog-ng/syslog-ng/pull/4549))

  * `python`: Fixed a crash when reloading with a config, which uses a python parser with multiple references.
    ([#4552](https://github.com/syslog-ng/syslog-ng/pull/4552))
    ([#4567](https://github.com/syslog-ng/syslog-ng/pull/4567))

  * `mqtt()`: Fixed the name of the stats instance (`mqtt-source`) to conform to the standard comma-separated format.
    ([#4551](https://github.com/syslog-ng/syslog-ng/pull/4551))

  * metrics: Fixed a memory leak which happened during reload, and was introduced in 4.3.0.
    ([#4568](https://github.com/syslog-ng/syslog-ng/pull/4568))

## Packaging

  * scl.conf: The scl.conf file has been moved to /share/syslog-ng/include/scl.conf
    ([#4534](https://github.com/syslog-ng/syslog-ng/pull/4534))

  * C++ plugins: Some of syslog-ng's plugins now contain C++ code.

    By default they are being built if a C++ compiler is available.
    Disabling it is possible with `--disable-cpp`.

    Affected plugins:
      * `lib/syslog-ng/libexamples.so`
        * `--disable-cpp` will only disable the C++ part (`random-choice-generator()`)
      * `lib/syslog-ng/libotel.so`

    ([#4484](https://github.com/syslog-ng/syslog-ng/pull/4484))

  * `debian`: A new module is added, called syslog-ng-mod-grpc.

    Its dependencies are: `protobuf-compiler`, `protobuf-compiler-grpc`, `libprotobuf-dev`, `libgrpc++-dev`.
    Building the module can be toggled with `--enable-grpc`.
    ([#4510](https://github.com/syslog-ng/syslog-ng/pull/4510))

  * pcre: syslog-ng now uses pcre2 (8 bit) as a dependency instead of pcre.

    The minimum pcre2 version is 10.0.
    ([#4537](https://github.com/syslog-ng/syslog-ng/pull/4537))

## Notes to developers

  * `lib/logmsg`: Public field `LogMessage::protected` has been renamed to `LogMessage::write_protected`.

    Direct usage of this field is discouraged, instead use the following functions:
      * `log_msg_is_write_protected()`
      * `log_msg_write_protect()`
    ([#4484](https://github.com/syslog-ng/syslog-ng/pull/4484))

  * `lib/templates`: Public field `LogTemplate::template` has been renamed to `LogTemplate::template_str`.
    ([#4484](https://github.com/syslog-ng/syslog-ng/pull/4484))


## Other changes

  * `syslog-ng-cfg-db`: Moved to a separate repository.

    It is available at: https://github.com/alltilla/syslog-ng-cfg-helper
    ([#4475](https://github.com/syslog-ng/syslog-ng/pull/4475))

  * `disk-buffer`: Added alternative option names

    `disk-buf-size()` -> `capacity-bytes()`
    `qout-size()` -> `front-cache-size()`
    `mem-buf-length()` -> `flow-control-window-size()`
    `mem-buf-size()` -> `flow-control-window-bytes()`

    Old option names are still available.

    Example configs:
    ```
    tcp(
      "127.0.0.1" port(2001)
      disk-buffer(
        reliable(yes)
        capacity-bytes(1GiB)
        flow-control-window-bytes(200MiB)
        front-cache-size(1000)
      )
    );

    tcp(
      "127.0.0.1" port(2001)
      disk-buffer(
        reliable(no)
        capacity-bytes(1GiB)
        flow-control-window-size(10000)
        front-cache-size(1000)
      )
    );
    ```
    ([#4526](https://github.com/syslog-ng/syslog-ng/pull/4526))

  * selinux: Added RHEL9 support for the selinux policies

    Added RHEL9 support for the selinux policies at `contrib/selinux`
    ([#4509](https://github.com/syslog-ng/syslog-ng/pull/4509))

  * metrics: replace `driver_instance` (`stats_instance`) with metric labels

    The new metric system had a label inherited from legacy: `driver_instance`.

    This non-structured label has been removed and different driver-specific labels have been added instead, for example:

    Before:
    ```
    syslogng_output_events_total{driver_instance="mongodb,localhost:27017,defaultdb,,coll",id="#anon-destination1#1",result="queued"} 4
    ```

    After:
    ```
    syslogng_output_events_total{driver="mongodb",host="localhost:27017",database="defaultdb",collection="coll",id="#anon-destination1#1",result="queued"} 4
    ```

    This change may affect legacy stats outputs (`syslog-ng-ctl stats`), for example, `persist-name()`-based naming
    is no longer supported in this old format.
    ([#4551](https://github.com/syslog-ng/syslog-ng/pull/4551))

  * APT packages: Added Ubuntu Lunar Lobster and Debian Bookworm support.
    ([#4561](https://github.com/syslog-ng/syslog-ng/pull/4561))

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

Andreas Friedmann, Attila Szakacs, Balazs Scheidler, Bálint Horváth,
Chuck Silvers, Evan Rempel, Hofi, Kovacs, Gergo Ferenc, László Várady,
Romain Tartière, Ryan Faircloth, vostrelt