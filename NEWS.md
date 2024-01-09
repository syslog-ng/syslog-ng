4.6.0
=====

Read Axoflow's [blog post](https://axoflow.com/axosyslog-release-4-6/) for more details.
You can read more about the new features in the AxoSyslog [documentation](https://axoflow.com/docs/axosyslog-core/).


## Highlights

### Forwarding logs to Google BigQuery

The `bigquery()` destination inserts logs to a Google BigQuery table via the
high-performance gRPC API.

Authentication is done via [Application Default Credentials](https://cloud.google.com/docs/authentication/provide-credentials-adc).

You can locate your BigQuery table with the `project()` `dataset()` and `table()`
options.

There are two ways to configure your table's schema.
  - You can set the columns and their respective type and template with the
    `schema()` option. The available types are: `STRING`, `BYTES`, `INTEGER`,
    `FLOAT`, `BOOLEAN`, `TIMESTAMP`, `DATE`, `TIME`, `DATETIME`, `JSON`,
    `NUMERIC`, `BIGNUMERIC`, `GEOGRAPHY`, `RECORD`, `INTERVAL`.
  - Alternatively you can import a `.proto` file with the `protobuf-schema()` option,
    and map the templates for each column.

The performance can be further improved with the `workers()`, `batch-lines()`,
`batch-bytes()`, `batch-timeout()` and `compression()` options. By default the
messages are sent with one worker, one message per batch and without compression.

Keepalive can be configured with the `keep-alive()` block and its `time()`,
`timeout()`  and `max-pings-without-data()` options.

Example config:
```
bigquery(
    project("test-project")
    dataset("test-dataset")
    table("test-table")
    workers(8)

    schema(
        "message" => "$MESSAGE"
        "app" STRING => "$PROGRAM"
        "host" STRING => "$HOST"
        "pid" INTEGER => int("$PID")
    )

    on-error("drop-property")

    # or alternatively instead of schema():
    # protobuf-schema("/tmp/test.proto"
    #                 => "$MESSAGE", "$PROGRAM", "$HOST", "$PID")

    # keep-alive(time(20000) timeout(10000) max-pings-without-data(0))
);
```

Example `.proto` schema:
```
syntax = "proto2";
​
message CustomRecord {
  optional string message = 1;
  optional string app = 2;
  optional string host = 3;
  optional int64 pid = 4;
}
```

([#4733](https://github.com/syslog-ng/syslog-ng/pull/4733))
([#4770](https://github.com/syslog-ng/syslog-ng/pull/4770))
([#4756](https://github.com/syslog-ng/syslog-ng/pull/4756))


### Collecting native macOS system logs

Two new sources have been added on macOS: `darwin-oslog()`, `darwin-oslog-stream()`.
`darwin-oslog()` replaced the earlier file source based solution with a native OSLog
framework based one, and is automatically used in the `system()` source on darwin
platform if the **darwinosl** plugin is presented.

This plugin is available only on macOS 10.15 Catalina and above, the first version
that has the OSLog API.

#### `darwin-oslog()`

This is a native OSLog Framework based source to read logs from the local store of
the unified logging system on darwin OSes.
For more info, see https://developer.apple.com/documentation/oslog?language=objc

The following parameters can be used for customization:
  - `filter-predicate()`
      - string value, which can be used to filter the log messages natively
      - default value: `(eventType == 'logEvent' || eventType == 'lossEvent' || eventType == 'stateEvent' || eventType == 'userActionEvent') && (logType != 'debug')`
      - for more details, see
          - `man log`
          - https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/Predicates/Articles/pSyntax.html
  - `go-reverse()`
      - boolean value, setting to `yes` will provide a reverse-ordered log list
        (from latest to oldest)
      - default value: `no`
  - `do-not-use-bookmark()`
      - boolean value, setting to `yes` will prevent syslog-ng from continuing to
        feed the logs from the last remembered position after a (re-)start, which means,
        depending on the other settings, the feed will always start from the end/beginning
        of the available log list
      - default value: `no`, which means syslog-ng will attempt to continue feeding from
        the last remembered log position after a (re-)start
  - `max-bookmark-distance()`
      - integer value, maximum distance in seconds that far an earlier bookmark can point
        backward, e.g. if syslog-ng was stopped for 10 minutes and max-bookmark-distance
        is set to 60 then syslog-ng will start feeding the logs only from the last 60
        seconds at startup, 9 minutes of logs 'will be lost'
      - default value: `0`, which means no limit
  - `read-old-records()`
      - boolean value, controls if syslog-ng should start reading logs from the oldest
        available at first start (or if no bookmark can be found)
      - default value: `no`
  - `fetch-delay()`
      - integer value, controls how much time syslog-ng should wait between reading/sending
        log messages, this is a fraction of a second, where wait_time = 1 second / n, so,
        e.g. n=1 means that only about 1 log will be read and sent in each second,
        and n=1 000 000 means only 1 microsecond (the allowed minimum value now!)
        will be the delay between read/write attempts
      - Use with care, though lower delay time can increase log feed performance, at the
        same time could lead to a heavy system load!
      - default value: `10 000`
  - `fetch-retry-delay()`
      - integer value, controls how many seconds syslog-ng will wait before a repeated
        attempt to read/send once it's out of available logs
      - default value: `1`
  - `log-fetch-limit()`
      - **Warning**: _This option is now disabled due to an OSLog API bug_
        _(https://openradar.appspot.com/radar?id=5597032077066240), once it's fixed it_
        _will be enabled again_
      - integer value, that limits the number of logs syslog-ng will send in one run
      - default value: `0`, which means no limit

NOTE: the persistent OSLog store is not infinite, depending on your system setting usually,
it keeps about 7 days of logs on disk, so it could happen that the above options cannot
operate the way you expect, e.g. if syslog-ng was stopped for about more then a week it
could happen that will not be able to restart from the last saved bookmark position
(as that might not be presented in the persistent log anymore)

#### `darwin-oslog-stream()`

This is a wrapper around the OS command line "log stream" command that can provide a live
log stream feed. Unlike in the case of `darwin-oslog()` the live stream can contain
non-persistent log events too, so take care, there might be a huge number of log events
every second that could put an unusual load on the device running syslog-ng with this source.
Unfortunately, there's no public API to get the same programmatically, so this one is
implemented using a program() source.

Possible parameters:
  - `params()`
    - a string that can contain all the possible params the macOS `log` tool can accept
    - see `log --help stream` for full reference, and `man log` for more details
    - IMPORTANT: the parameter `--style` is used internally (defaults to `ndjson`), so it
      cannot be overridden, please use other sysylog-ng features (templates, rewrite rules, etc.)
      for final output formatting
    - default value: `--type log --type trace --level info --level debug`,
      you can use \``def-osl-stream-params`\` for referencing it if you wish to keep the
      defaults when you add your own

([#4423](https://github.com/syslog-ng/syslog-ng/pull/4423))

### Collecting qBittorrent logs

The new `qbittorrent()` source, reads qBittorrent logs from its log file output.

Example minimal config:
```
source s_qbittorrent {
  qbittorrent(
    dir("/path/to/my/qbittorrent/root/log/dir")
  );
};
```

The root dir of the qBittorrent logs can be found in the
"Tools" / "Preferences" / "Behavior" / "Log file" / "Save path" field.

As the `qbittorrent()` source is based on a `file()` source, all of the `file()`
source options are applicable, too.

([#4760](https://github.com/syslog-ng/syslog-ng/pull/4760))

### Collecting pihole FTL logs

The new `pihole-ftl()` source reads pihole FTL (Faster Than Light) logs, which
are usually accessible in the "Tools" / "Pi-hole diagnosis" menu.

Example minimal config:
```
source s_pihole_ftl {
  pihole-ftl();
};
```

By default it reads the `/var/log/pihole/FTL.log` file.
You can change the root dir of Pi-hole's logs with the `dir()` option,
where the `FTL.log` file can be found.

As the `pihole-ftl()` source is based on a `file()` source, all of the
`file()` source options are applicable, too.

([#4760](https://github.com/syslog-ng/syslog-ng/pull/4760))

### Parsing Windows Eventlog XMLs

The new `windows-eventlog-xml-parser()` introduces parsing support for Windows Eventlog XMLs.

Its parameters are the same as the `xml()` parser.

Example config:
```
parser p_win {
    windows-eventlog-xml-parser(prefix(".winlog."));
};
```

([#4793](https://github.com/syslog-ng/syslog-ng/pull/4793))


## Features

  * `cloud-auth()`: Added support for `user-managed-service-account()` `gcp()` auth method.

    This authentication method can be used on VMs in GCP to use the linked service.

    Example minimal config, which tries to use the "default" service account:
    ```
    cloud-auth(
      gcp(
        user-managed-service-account()
      )
    )
    ```

    Full config:
    ```
    cloud-auth(
      gcp(
        user-managed-service-account(
          name("alltilla@syslog-ng-test-project.iam.gserviceaccount.com")
          metadata-url("my-custom-metadata-server:8080")
        )
      )
    )
    ```

    This authentication method is extremely useful with syslog-ng's `google-pubsub()` destination,
    when it is running on VMs in GCP, for example:
    ```
    destination {
      google-pubsub(
        project("syslog-ng-test-project")
        topic("syslog-ng-test-topic")
        auth(user-managed-service-account())
      );
    };
    ```

    For more info about this GCP authentication method, see:
     * https://cloud.google.com/compute/docs/access/authenticate-workloads#curl
     * https://cloud.google.com/compute/docs/access/create-enable-service-accounts-for-instances
    ([#4755](https://github.com/syslog-ng/syslog-ng/pull/4755))

  * `opentelemetry()`, `syslog-ng-otlp()` sources: Added `workers()` option.

    This feature enables processing the OTLP messages on multiple threads,
    which can greatly improve the performance.
    By default it is set to `workers(1)`.
    ([#4774](https://github.com/syslog-ng/syslog-ng/pull/4774))

  * `opentelemetry()`, `syslog-ng-otlp()` destinations: Added `compression()` option.

    This boolean option can be used to enable gzip compression in gRPC requests.
    By default it is set to `compression(no)`.
    ([#4765](https://github.com/syslog-ng/syslog-ng/pull/4765))

  * `opentelemetry()`, `syslog-ng-otlp()` destinations: Added `batch-bytes()` option.

    This option lets the user limit the bytes size of a batch. As there is a
    default 4 MiB batch limit by OTLP, it is necessary to keep the batch size
    smaller, but it would be hard to configure without this option.

    Please note that the batch can be at most 1 message larger than the set
    limit, so consider this when setting this value.

    The default value is 4 MB, which is a bit below 4 MiB.

    The calculation of the batch size is done before compression, which is
    the same as the limit is calculated on the server.

    Example config:
    ```
      syslog-ng-otlp(
        url("localhost:12345")
        workers(16)
        log-fifo-size(1000000)

        batch-timeout(5000) # ms
        batch-lines(1000000) # Huge limit, batch-bytes() will limit us sooner

        batch-bytes(1MB) # closes and flushes the batch after the last message pushed it above the 1 MB limit
        # not setting batch-bytes() defaults to 4 MB, which is a bit below the default 4 MiB limit
      );
    ```
    ([#4772](https://github.com/syslog-ng/syslog-ng/pull/4772))

  * `opentelemetry()`, `syslog-ng-otlp()`: Added syslog-ng style list support.
    ([#4794](https://github.com/syslog-ng/syslog-ng/pull/4794))

  * `$(tag)` template function: expose bit-like tags that are set on messages.

    Syntax:
        `$(tag <name-of-the-tag> <value-if-set> <value-if-unset>)`

    Unless the value-if-set/unset arguments are specified `$(tag)` results in a
    boolean type, expanding to "0" or "1" depending on whether the message has
    the specified tag set.

    If value-if-set/unset are present, `$(tag)` would return a string, picking the
    second argument `<value-if-set>` if the message has `<tag>` and picking the
    third argument `<value-if-unset>` if the message does not have `<tag>`
    ([#4766](https://github.com/syslog-ng/syslog-ng/pull/4766))

  * `set-severity()` support for aliases: widespread aliases to severity values
    produced by various applications are added to set-severity().
    ([#4763](https://github.com/syslog-ng/syslog-ng/pull/4763))

  * `flags(seqnum-all)`: available in all destination drivers, this new flag
    changes `$SEQNUM` behaviour, so that all messages get a sequence number, not
    just local ones.  Previously syslog-ng followed the logic of the RFC5424
    meta.sequenceId structured data element, e.g.  only local messages were to
    get a sequence number, forwarded messages retained their original sequenceId
    that we could potentially receive ourselves.

    For example, this destination would include the meta.sequenceId SDATA
    element even for non-local logs and increment that value by every message
    transmitted:

      `destination { syslog("127.0.0.1" port(2001) flags(seqnum-all)); };`

    This generates a message like this on the output, even if the message is
    not locally generated (e.g. forwarded from another syslog sender):

    ```
      <13>1 2023-12-09T21:51:30+00:00 localhost sdff - - [meta sequenceId="1"] f sdf fsd
      <13>1 2023-12-09T21:51:32+00:00 localhost sdff - - [meta sequenceId="2"] f sdf fsd
      <13>1 2023-12-09T21:51:32+00:00 localhost sdff - - [meta sequenceId="3"] f sdf fsd
      <13>1 2023-12-09T21:51:32+00:00 localhost sdff - - [meta sequenceId="4"] f sdf fsd
      <13>1 2023-12-09T21:51:32+00:00 localhost sdff - - [meta sequenceId="5"] f sdf fsd
    ```
    ([#4745](https://github.com/syslog-ng/syslog-ng/pull/4745))

  * `loggen`: improve loggen performance for synthetic workloads, so we can test
    for example up to 650k msg/sec on a AMD Ryzen 7 Pro 6850U CPU.
    ([#4476](https://github.com/syslog-ng/syslog-ng/pull/4476))


## Bugfixes

  * `metrics-probe()`: Fixed not cleaning up dynamic labels for each message if no static labels are set.
    ([#4750](https://github.com/syslog-ng/syslog-ng/pull/4750))

  * `regexp-parser()`: Fixed a bug, which stored some values incorrectly if `${MESSAGE}` was changed with a capture group.
    ([#4759](https://github.com/syslog-ng/syslog-ng/pull/4759))

  * `network()` source: fix marking originally valid utf-8 messages when `sanitize-utf8` is enabled
    ([#4744](https://github.com/syslog-ng/syslog-ng/pull/4744))

  * `python()`: Fixed a memory leak in `list` typed `LogMessage` values.
    ([#4790](https://github.com/syslog-ng/syslog-ng/pull/4790))

## Packaging

  * `VERSION` renamed to `VERSION.txt`: due to a name collision with C++ based
    builds on MacOS, the file containing our version number was renamed to
    VERSION.txt.
    ([#4775](https://github.com/syslog-ng/syslog-ng/pull/4775))

  * Added `gperf` as a build dependency.
    ([#4763](https://github.com/syslog-ng/syslog-ng/pull/4763))


## Notes to developers

  * `LogThreadedSourceDriver`: Added multi-worker API, which is a breaking change.

    Check the Pull Request for inspiration on how to follow up these changes.
    ([#4774](https://github.com/syslog-ng/syslog-ng/pull/4774))


## Other changes

  * `network()`/`syslog()` sources: support UTF-8 sanitization/validation of RFC 5424 and `no-parse` messages

    The `sanitize-utf8`, `validate-utf8` flags are now supported when parsing RFC 5424 messages or when parsing is disabled.
    ([#4744](https://github.com/syslog-ng/syslog-ng/pull/4744))

  * APT packages: Added Ubuntu Mantic Minotaur.
    ([#4737](https://github.com/syslog-ng/syslog-ng/pull/4737))

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

Attila Szakacs, Balazs Scheidler, Hofi, László Várady, Romain Tartière
