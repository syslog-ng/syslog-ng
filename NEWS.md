4.4.0
=====

Read Axoflow's [blog post](https://axoflow.com/axosyslog-release-4-4/) for more details.
You can read more about the new features in the AxoSyslog [documentation](https://axoflow.com/docs/axosyslog-core/).


## Highlights

### Sending messages between syslog-ng instances via OTLP/gRPC

The `syslog-ng-otlp()` source and destination helps to transfer the internal representation
of a log message between syslog-ng instances. In contrary to the `syslog-ng()` (`ewmm()`)
drivers, `syslog-ng-otlp()` does not transfer the messages on simple TCP connections, but uses
the OpenTelemetry protocol to do so.

It is easily scalable (`workers()` option), uses built-in application layer acknowledgement,
out of the box supports google service authentication (ADC or ALTS), and gives the possibility
of better load balancing.

The performance is currently similar to `ewmm()` (OTLP is ~30% quicker) but there is a source
side limitation, which will be optimized. We measured 200-300% performance improvement with a
PoC optimized code using multiple threads, so stay tuned.

Note: The `syslog-ng-otlp()` source is only an alias to the `opentelemetry()` source.
This is useful for not needing to open different ports for the syslog-ng messages and other
OpenTelemetry messages. The syslog-ng messages are marked with a `@syslog-ng` scope name and
the current syslog-ng version as the scope version. Both sources will handle the incoming
syslog-ng messages as syslog-ng messages, and all other messages as simple OpenTelemetry
messages.
([#4564](https://github.com/syslog-ng/syslog-ng/pull/4564))

### Grafana Loki destination

The `loki()` destination sends messages to Grafana Loki using gRPC.
The message format conforms to the documented HTTP endpoint:
https://grafana.com/docs/loki/latest/reference/api/#push-log-entries-to-loki

Example config:
```
loki(
    url("localhost:9096")
    labels(
        "app" => "$PROGRAM",
        "host" => "$HOST",
    )

    workers(16)
    batch-timeout(10000)
    batch-lines(1000)
);
```

Loki requires monotonic timestamps within the same label-set, which makes
it difficult to use the original message timestamp without the possibility
of message loss. In case the monotonic property is violated, Loki discards
the problematic messages with an error. The source of the timestamps can be
configured with the `timestamp()` option (`current`, `received`, `msg`).

([#4631](https://github.com/syslog-ng/syslog-ng/pull/4631))

### S3 destination

The `s3()` destination stores log messages in S3 objects.

Minimal config:
```
s3(
    url("http://localhost:9000")
    bucket("syslog-ng")
    access-key("my-access-key")
    secret-key("my-secret-key")
    object-key("${HOST}/my-logs")
    template("${MESSAGE}\n")
);
```

#### Compression

Setting `compression(yes)` enables gzip compression, and implicitly adds a `.gz` suffix to the
created object's key. Use the `compresslevel()` options to set the level of compression (0-9).

#### Rotation based on object size

The `max-object-size()` option configures syslog-ng to finish an object if it reaches a certain
size. syslog-ng will append an index (`"-1"`, `"-2"`, ...) to the end of the object key when
starting a new object after rotation.

#### Rotation based on timestamp

The `object-key-timestamp()` option can be used to set a datetime related template, which gets
appended to the end of the object (e.g. `"${R_MONTH_ABBREV}${R_DAY}"` => `"-Sep25"`). When a log
message arrives with a newer timestamp template resolution, the previous timestamped object gets
finised and a new one is started with the new timestamp. Backfill messages do not reopen and append
the old object, but starts a new object with the key having an index appended to the old object.

#### Rotation based on timeout

The `flush-grace-period()` option sets the number of minutes to wait for new messages to arrive to
objects, if the timeout expires the object is finished, and a new message will start a new with
an index appended.

#### Upload options

The objects are uploaded with the multipart upload API. Chunks are composed locally. When a chunk
reaches a certain size (by default 5 MiB), the chunk is uploaded. When an object is finished, the
multipart upload gets completed and the chunks are merged by S3.

Upload parameters can be configured with the `chunk-size()`, `upload-threads()` and
`max-pending-uploads()` options.


#### Additional options

Additional options include `region()`, `storage-class()` and `canned-acl()`.

([#4624](https://github.com/syslog-ng/syslog-ng/pull/4624))


## Features

  * `http()`: Added compression ability for use with metered egress/ingress

    The new features can be accessed with the following options:
     * `accept-encoding()` for requesting the compression of HTTP responses form the server.
       (These are currently not used by syslog-ng, but they still contribute to network traffic.)
       The available options are `identity` (for no compression), `gzip` or `deflate`.
       If you want the driver to accept multiple compression types, you can list them separated by
       commas inside the quotation mark, or write `all`, if you want to enable all available compression types.
     * `content-compression()` for compressing messages sent by syslog-ng. The available options are
       `identity` for no compression, `gzip`, or `deflate`.

    Below you can see a configuration example:
    ```
    destination d_http_compressed{
      http(url("127.0.0.1:80"), content-compression("deflate"), accept-encoding("all"));
    };
    ```
    ([#4137](https://github.com/syslog-ng/syslog-ng/pull/4137))

  * `opensearch`: Added a new destination.

    It is similar to `elasticsearch-http()`, with the difference that it does not have the `type()`
    option, which is deprecated and advised not to use.
    ([#4560](https://github.com/syslog-ng/syslog-ng/pull/4560))

  * Added metrics for message delays: a new metric is introduced that measures the
    delay the messages accumulate while waiting to be delivered by syslog-ng.
    The measurement is sampled, e.g. syslog-ng would take the very first message
    in every second and expose its delay as a value of the new metric.

    There are two new metrics:
      * syslogng_output_event_delay_sample_seconds -- contains the latency of
        outgoing messages
      * syslogng_output_event_delay_sample_age_seconds -- contains the age of the last
        measurement, relative to the current time.
    ([#4565](https://github.com/syslog-ng/syslog-ng/pull/4565))

  * `metrics-probe`: Added dynamic labelling support via name-value pairs

    You can use all value-pairs options, like `key()`, `rekey()`, `pair()` or `scope()`, etc...

    Example:
    ```
    metrics-probe(
      key("foo")
      labels(
        "static-label" => "bar"
        key(".my_prefix.*" rekey(shift-levels(1)))
      )
    );
    ```
    ```
    syslogng_foo{static_label="bar",my_prefix_baz="almafa",my_prefix_foo="bar",my_prefix_nested_axo="flow"} 4
    ```
    ([#4610](https://github.com/syslog-ng/syslog-ng/pull/4610))

  * `systemd-journal()`: Added support for enabling multiple systemd-journal() sources

    Using multiple systemd-journal() sources are now possible as long as each source uses a unique
    systemd namespace. The namespace can be configured with the `namespace()`` option, which has a
    default value of `"*"`.
    ([#4553](https://github.com/syslog-ng/syslog-ng/pull/4553))

  * `stdout()`: added a new destination that allows you to write messages easily
    to syslog-ng's stdout.
    ([#4620](https://github.com/syslog-ng/syslog-ng/pull/4620))

  * `network()`: Added `ignore-hostname-mismatch` as a new flag to `ssl-options()`.

    By specifying `ignore-hostname-mismatch`, you can ignore the subject name of a
    certificate during the validation process. This means that syslog-ng will
    only check if the certificate itself is trusted by the current set of trust
    anchors (e.g. trusted CAs) ignoring the mismatch between the targeted
    hostname and the certificate subject.
    ([#4628](https://github.com/syslog-ng/syslog-ng/pull/4628))


## Bugfixes

  * `syslog-ng`: fix runtime `undefined symbol: random_choice_generator_parser'` when executing `syslog-ng -V` or
    using an example plugin
    ([#4615](https://github.com/syslog-ng/syslog-ng/pull/4615))

  * Fix threaded destination crash during a configuration revert

    Threaded destinations that do not support the `workers()` option crashed while
    syslog-ng was trying to revert to an old configuration.
    ([#4588](https://github.com/syslog-ng/syslog-ng/pull/4588))

  * `redis()`: fix incrementing seq_num
    ([#4588](https://github.com/syslog-ng/syslog-ng/pull/4588))

  * `python()`: fix crash when using `Persist` or `LogTemplate` without global `python{}` code block in configuration
    ([#4572](https://github.com/syslog-ng/syslog-ng/pull/4572))

  * `mqtt()` destination: fix template option initialization
    ([#4605](https://github.com/syslog-ng/syslog-ng/pull/4605))

  * `opentelemetry`: Fixed error handling in case of insert failure.
    ([#4583](https://github.com/syslog-ng/syslog-ng/pull/4583))

  * pdbtool: add validation for types of `<value>` tags

    In patterndb, you can add extra name-value pairs following a match with the tags.
    But the actual value of these name-value pairs were never validated against their types,
    meaning that an incorrect value could be set using this construct.
    ([#4621](https://github.com/syslog-ng/syslog-ng/pull/4621))

  * `grouping-by()`, `group-lines()`: Fixed a persist name generating error.
    ([#4478](https://github.com/syslog-ng/syslog-ng/pull/4478))


## Packaging

  * debian: Added tzdata-legacy to BuildDeps for recent debian versions.

    In the recent debian packaging some of the timezone info files moved
    to a new tzdata-legacy package from the standard tzdata package.
    ([#4643](https://github.com/syslog-ng/syslog-ng/pull/4643))

  * rhel: `contrib/vim` has been removed from the source.
    ([#4607](https://github.com/syslog-ng/syslog-ng/pull/4607))


## Other changes

  * APT packages: Dropped support for Ubuntu Bionic.
  ([#4648](https://github.com/syslog-ng/syslog-ng/pull/4648))

  * `vim`: Syntax highlight file is no longer packaged.

    vim syntax files where previously installed by the RedHat packages of syslog-ng
    (but not the Debian ones). These files where sometime lagging behind, so in order
    to provide a more up-to-date experience on all platforms, regardless of the
    installation of the syslog-ng package, the vim syntax files have been moved to a
    dedicated repository [syslog-ng/vim-syslog-ng](https://github.com/syslog-ng/vim-syslog-ng) that can be used using a plugin manager such as
    [vim-plug](https://github.com/junegunn/vim-plug), [vim-pathogen](https://github.com/tpope/vim-pathogen) or [vundle](https://github.com/VundleVim/Vundle.vim).
    ([#4607](https://github.com/syslog-ng/syslog-ng/pull/4607))


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

Alex Becker, Attila Szakacs, Balazs Scheidler, Bálint Horváth, Hofi,
László Várady, Romain Tartière, Szilard Parrag
