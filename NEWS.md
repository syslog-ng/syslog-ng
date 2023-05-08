4.2.0
=====

Read Axoflow's [blog post](https://axoflow.com/axosyslog-release-4-2/) for more details.

## Highlights

#### Sending messages to Splunk HEC
The `splunk-hec-event()` destination feeds Splunk via the [HEC events API](https://docs.splunk.com/Documentation/Splunk/9.0.4/RESTREF/RESTinput#services.2Fcollector.2Fevent.2F1.0).

Minimal config:
```
destination d_splunk_hec_event {
  splunk-hec-event(
    url("https://localhost:8088")
    token("70b6ae71-76b3-4c38-9597-0c5b37ad9630")
  );
};
```

Additional options include:
  * `event()`
  * `index()`
  * `source()`
  * `sourcetype()`
  * `host()`
  * `time()`
  * `default-index()`
  * `default-source()`
  * `default-sourcetype()`
  * `fields()`
  * `extra-headers()`
  * `extra-queries()`
  * `content-type()`


The `splunk-hec-raw()` destination feeds Splunk via the [HEC raw API](https://docs.splunk.com/Documentation/Splunk/9.0.4/RESTREF/RESTinput#services.2Fcollector.2Fraw.2F1.0).

Minimal config:
```
destination d_splunk_hec_raw {
  splunk-hec-raw(
    url("https://localhost:8088")
    token("70b6ae71-76b3-4c38-9597-0c5b37ad9630")
    channel("05ed4617-f186-4ccd-b4e7-08847094c8fd")
  );
};
```

([#4462](https://github.com/syslog-ng/syslog-ng/pull/4462))

#### Smart multi-line for recognizing backtraces
`multi-line-mode(smart)`:
With this multi-line mode, the inherently multi-line data backtrace format is
recognized even if they span multiple lines in the input and are converted
to a single log message for easier analysis.  Backtraces for the following
programming languages are recognized : Python, Java, JavaScript, PHP, Go,
Ruby and Dart.

The regular expressions to recognize these programming languages are
specified by an external file called
`/usr/share/syslog-ng/smart-multi-line.fsm` (installation path depends on
configure arguments), in a format that is described in that file.

`group-lines()` parser: this new parser correlates multi-line messages
received as separate, but subsequent lines into a single log message.
Received messages are first collected into streams related messages (using
key()), then collected into correlation contexts up to timeout() seconds.
The identification of multi-line messages are then performed on these
message contexts within the time period.

```
  group-lines(key("$FILE_NAME")
              multi-line-mode("smart")
        template("$MESSAGE")
        timeout(10)
        line-separator("\n")
  );
```

([#4225](https://github.com/syslog-ng/syslog-ng/pull/4225))

#### HYPR Audit Trail source
`hypr-audit-trail()` & `hypr-app-audit-trail()` source drivers are now
available to monitor the audit trails for [HYPR](https://www.hypr.com) applications.

See the README.md file in the driver's directory to see usage information.

([#4175](https://github.com/syslog-ng/syslog-ng/pull/4175))

#### `ebpf()` plugin and reuseport packet randomizer
A new ebpf() plugin was added as a framework to leverage the kernel's eBPF
infrastructure to improve performance and scalability of syslog-ng.

Example:

```
source s_udp {
        udp(so-reuseport(yes) port(2000) persist-name("udp1")
                ebpf(reuseport(sockets(4)))
        );
        udp(so-reuseport(yes) port(2000) persist-name("udp2"));
        udp(so-reuseport(yes) port(2000) persist-name("udp3"));
        udp(so-reuseport(yes) port(2000) persist-name("udp4"));
};
```

NOTE: The `ebpf()` plugin is considered advanced usage so its compilation is
disabled by default.  Please don't use it unless all other avenues of
configuration solutions are already tried.  You will need a special
toolchain and a recent kernel version to compile and run eBPF programs.

([#4365](https://github.com/syslog-ng/syslog-ng/pull/4365))


## Features

  * `network` source: During a TLS handshake, syslog-ng now automatically sets the
    `certificate_authorities` field of the certificate request based on the `ca-file()`
    and `ca-dir()` options. The `pkcs12-file()` option already had this feature.
    ([#4412](https://github.com/syslog-ng/syslog-ng/pull/4412))

  * `metrics-probe()`: Added `level()` option to set the stats level of the generated metrics.
    ([#4453](https://github.com/syslog-ng/syslog-ng/pull/4453))

  * `metrics-probe()`: Added `increment()` option.

    Users can now set a template, which resolves to a number that modifies
    the increment of the counter. If not set, the increment is 1.
    ([#4447](https://github.com/syslog-ng/syslog-ng/pull/4447))

  * `python`: Added support for typed custom options.

    This applies for `python` source, `python-fetcher` source, `python` destination,
    `python` parser and `python-http-header` inner destination.

    Example config:
    ```
    python(
      class("TestClass")
      options(
        "string_option" => "example_string"
        "bool_option" => True  # supported values are: True, False, yes, no
        "integer_option" => 123456789
        "double_option" => 123.456789
        "string_list_option" => ["string1", "string2", "string3"]
        "template_option" => LogTemplate("${example_template}")
      )
    );
    ```

    **Breaking change! Previously values were converted to strings if possible, now they are passed
    to the python class with their real type. Make sure to follow up these changes
    in your python code!**
    ([#4354](https://github.com/syslog-ng/syslog-ng/pull/4354))

  * `mongodb` destination: Added support for list, JSON and null types.
    ([#4437](https://github.com/syslog-ng/syslog-ng/pull/4437))

  * `add-contextual-data()`: significantly reduce memory usage for large CSV
    files.
    ([#4444](https://github.com/syslog-ng/syslog-ng/pull/4444))

  * `python()`: new LogMessage methods for querying as string and with default values

    - `get(key[, default])`
      Return the value for `key` if `key` exists, else `default`. If `default` is
      not given, it defaults to `None`, so that this method never raises a
      `KeyError`.

    - `get_as_str(key, default=None, encoding='utf-8', errors='strict', repr='internal')`:
      Return the string value for `key` if `key` exists, else `default`.
      If `default` is not given, it defaults to `None`, so that this method never
      raises a `KeyError`.

      The string value is decoded using the codec registered for `encoding`.
      `errors` may be given to set the desired error handling scheme.

      Note that currently `repr='internal'` is the only available representation.
      We may implement another more Pythonic representation in the future, so please
      specify the `repr` argument explicitly if you want to avoid future
      representation changes in your code.
    ([#4410](https://github.com/syslog-ng/syslog-ng/pull/4410))

  * `kubernetes()` source: Added support for json-file logging driver format.
    ([#4419](https://github.com/syslog-ng/syslog-ng/pull/4419))

  * The new `$RAWMSG_SIZE` hard macro can be used to query the original size of the
    incoming message in bytes.

    This information may not be available for all source drivers.
    ([#4440](https://github.com/syslog-ng/syslog-ng/pull/4440))

  * syslog-ng configuration identifier

    A new syslog-ng configuration keyword has been added, which allows specifying a config identifier. For example:
    ```
    @config-id: cfg-20230404-13-g02b0850fc
    ```

    This keyword can be used for config identification in managed environments, where syslog-ng instances and their
    configuration are deployed/generated automatically.

    `syslog-ng-ctl config --id` can be used to query the active configuration ID and the SHA256 hash of the full
    "preprocessed" syslog-ng configuration. For example:

    ```
    $ syslog-ng-ctl config --id
    cfg-20230404-13-g02b0850fc (08ddecfa52a3443b29d5d5aa3e5114e48dd465e195598062da9f5fc5a45d8a83)
    ```
    ([#4420](https://github.com/syslog-ng/syslog-ng/pull/4420))

  * `syslog-ng`: add `--config-id` command line option

    Similarly to `--syntax-only`, this command line option parses the configuration
    and then prints its ID before exiting.

    It can be used to query the ID of the current configuration persisted on
    disk.
    ([#4435](https://github.com/syslog-ng/syslog-ng/pull/4435))

  * Health metrics and `syslog-ng-ctl healthcheck`

    A new `syslog-ng-ctl` command has been introduced, which can be used to query a healthcheck status from syslog-ng.
    Currently, only 2 basic health values are reported.

    `syslog-ng-ctl healthcheck --timeout <seconds>` can be specified to use it as a boolean healthy/unhealthy check.

    Health checks are also published as periodically updated metrics.
    The frequency of these checks can be configured with the `stats(healthcheck-freq())` option.
    The default is 5 minutes.
    ([#4362](https://github.com/syslog-ng/syslog-ng/pull/4362))

  * `$(format-json)` and template functions which support value-pairs
    expressions: new key transformations upper() and lower() have been added to
    translate the caps of keys while formatting the output template. For
    example:

        template("$(format-json test.* --upper)\n")

    Would convert all keys to uppercase. Only supports US ASCII.
    ([#4452](https://github.com/syslog-ng/syslog-ng/pull/4452))

  * `python()`, `python-fetcher()` sources: Added a mapping for the `flags()` option.

    The state of the `flags()` option is mapped to the `self.flags` variable, which is
    a `Dict[str, bool]`, for example:
    ```python
    {
        'parse': True,
        'check-hostname': False,
        'syslog-protocol': True,
        'assume-utf8': False,
        'validate-utf8': False,
        'sanitize-utf8': False,
        'multi-line': True,
        'store-legacy-msghdr': True,
        'store-raw-message': False,
        'expect-hostname': True,
        'guess-timezone': False,
        'header': True,
        'rfc3164-fallback': True,
    }
    ```
    ([#4455](https://github.com/syslog-ng/syslog-ng/pull/4455))


### Metrics
  * `network()`, `syslog()`: TCP connection metrics

    ```
    syslogng_socket_connections{id="tcp_src#0",driver_instance="afsocket_sd.(stream,AF_INET(0.0.0.0:5555))",direction="input"} 3
    syslogng_socket_max_connections{id="tcp_src#0",driver_instance="afsocket_sd.(stream,AF_INET(0.0.0.0:5555))",direction="input"} 10
    syslogng_socket_rejected_connections_total{id="tcp_src#0",driver_instance="afsocket_sd.(stream,AF_INET(0.0.0.0:5555))",direction="input"} 96
    ```

    `internal()`: `internal_events_queue_capacity` metric

    `syslog-ng-ctl healthcheck`: new healthcheck value `syslogng_internal_events_queue_usage_ratio`
    ([#4411](https://github.com/syslog-ng/syslog-ng/pull/4411))

  * `metrics`: new network (TCP, UDP) metrics are available on stats level 1

    ```
    # syslog-ng-ctl stats prometheus

    syslogng_socket_receive_buffer_used_bytes{id="#anon-source0#3",direction="input",driver_instance="afsocket_sd.udp4"} 0
    syslogng_socket_receive_buffer_max_bytes{id="#anon-source0#3",direction="input",driver_instance="afsocket_sd.udp4"} 268435456
    syslogng_socket_receive_dropped_packets_total{id="#anon-source0#3",direction="input",driver_instance="afsocket_sd.udp4"} 619173

    syslogng_socket_connections{id="#anon-source0#0",direction="input",driver_instance="afsocket_sd.(stream,AF_INET(0.0.0.0:2000))"} 1
    ```
    ([#4374](https://github.com/syslog-ng/syslog-ng/pull/4374))

  * New configuration-related metrics:

    ```
    syslogng_last_config_reload_timestamp_seconds 1681309903
    syslogng_last_successful_config_reload_timestamp_seconds 1681309758
    syslogng_last_config_file_modification_timestamp_seconds 1681309877
    ```
    ([#4420](https://github.com/syslog-ng/syslog-ng/pull/4420))

  * destination: Introduced queue metrics.

      * The corresponding driver is identified with the "id" and "driver_instance" labels.
      * Available counters are "memory_usage_bytes" and "events".
      * Memory queue metrics are available with "syslogng_memory_queue_" prefix,
        `disk-buffer` metrics are available with "syslogng_disk_queue_" prefix.
      * `disk-buffer` metrics have an additional "path" label, pointing to the location of the disk-buffer file
        and a "reliable" label, which can be either "true" or "false".
      * Threaded destinations, like `http`, `python`, etc have an additional "worker" label.

    Example metrics
    ```
    syslogng_disk_queue_events{driver_instance="http,http://localhost:1239",id="d_http_disk_buffer#0",path="/var/syslog-ng/syslog-ng-00000.rqf",reliable="true",worker="0"} 80
    syslogng_disk_queue_events{driver_instance="http,http://localhost:1239",id="d_http_disk_buffer#0",path="/var/syslog-ng/syslog-ng-00001.rqf",reliable="true",worker="1"} 7
    syslogng_disk_queue_events{driver_instance="http,http://localhost:1239",id="d_http_disk_buffer#0",path="/var/syslog-ng/syslog-ng-00002.rqf",reliable="true",worker="2"} 7
    syslogng_disk_queue_events{driver_instance="http,http://localhost:1239",id="d_http_disk_buffer#0",path="/var/syslog-ng/syslog-ng-00003.rqf",reliable="true",worker="3"} 7
    syslogng_disk_queue_events{driver_instance="tcp,localhost:1235",id="d_network_disk_buffer#0",path="/var/syslog-ng/syslog-ng-00000.qf",reliable="false"} 101
    syslogng_disk_queue_memory_usage_bytes{driver_instance="http,http://localhost:1239",id="d_http_disk_buffer#0",path="/var/syslog-ng/syslog-ng-00000.rqf",reliable="true",worker="0"} 3136
    syslogng_disk_queue_memory_usage_bytes{driver_instance="http,http://localhost:1239",id="d_http_disk_buffer#0",path="/var/syslog-ng/syslog-ng-00001.rqf",reliable="true",worker="1"} 2776
    syslogng_disk_queue_memory_usage_bytes{driver_instance="http,http://localhost:1239",id="d_http_disk_buffer#0",path="/var/syslog-ng/syslog-ng-00002.rqf",reliable="true",worker="2"} 2760
    syslogng_disk_queue_memory_usage_bytes{driver_instance="http,http://localhost:1239",id="d_http_disk_buffer#0",path="/var/syslog-ng/syslog-ng-00003.rqf",reliable="true",worker="3"} 2776
    syslogng_disk_queue_memory_usage_bytes{driver_instance="tcp,localhost:1235",id="d_network_disk_buffer#0",path="/var/syslog-ng/syslog-ng-00000.qf",reliable="false"} 39888
    syslogng_memory_queue_events{driver_instance="http,http://localhost:1236",id="d_http#0",worker="0"} 15
    syslogng_memory_queue_events{driver_instance="http,http://localhost:1236",id="d_http#0",worker="1"} 14
    syslogng_memory_queue_events{driver_instance="tcp,localhost:1234",id="d_network#0"} 29
    syslogng_memory_queue_memory_usage_bytes{driver_instance="http,http://localhost:1236",id="d_http#0",worker="0"} 5896
    syslogng_memory_queue_memory_usage_bytes{driver_instance="http,http://localhost:1236",id="d_http#0",worker="1"} 5552
    syslogng_memory_queue_memory_usage_bytes{driver_instance="tcp,localhost:1234",id="d_network#0"} 11448
    ```
    ([#4392](https://github.com/syslog-ng/syslog-ng/pull/4392))

  * `network()`, `syslog()`, `file()`, `http()`: new byte-based metrics for incoming/outgoing events

    These metrics show the serialized message sizes (protocol-specific header/framing/etc. length is not included).

    ```
    syslogng_input_event_bytes_total{id="s_network#0",driver_instance="tcp,127.0.0.1"} 1925529600
    syslogng_output_event_bytes_total{id="d_network#0",driver_instance="tcp,127.0.0.1:5555"} 565215232
    syslogng_output_event_bytes_total{id="d_http#0",driver_instance="http,http://127.0.0.1:8080/"} 1024
    ```
    ([#4440](https://github.com/syslog-ng/syslog-ng/pull/4440))

  * `disk-buffer`: Added metrics for monitoring the available space in disk-buffer `dir()`s.

    Metrics are available from `stats(level(1))`.

    By default, the metrics are generated every 5 minutes, but it can be changed in the global options:
    ```
    options {
      disk-buffer(
        stats(
          freq(10)
        )
      );
    };
    ```
    Setting `freq(0)` disabled this feature.

    Example metrics:
    ```
    syslogng_disk_queue_dir_available_bytes{dir="/var/syslog-ng"} 870109413376
    ```
    ([#4399](https://github.com/syslog-ng/syslog-ng/pull/4399))

  * `disk-buffer`: Added metrics for abandoned disk-buffer files.

    Availability is the same as the `disk_queue_dir_available_bytes` metric.

    Example metrics:
    ```
    syslogng_disk_queue_capacity_bytes{abandoned="true",path="/var/syslog-ng/syslog-ng-00000.rqf",reliable="true"} 104853504
    syslogng_disk_queue_disk_allocated_bytes{abandoned="true",path="/var/syslog-ng/syslog-ng-00000.rqf",reliable="true"} 273408
    syslogng_disk_queue_disk_usage_bytes{abandoned="true",path="/var/syslog-ng/syslog-ng-00000.rqf",reliable="true"} 269312
    syslogng_disk_queue_events{abandoned="true",path="/var/syslog-ng/syslog-ng-00000.rqf",reliable="true"} 860
    ```
    ([#4402](https://github.com/syslog-ng/syslog-ng/pull/4402))

  * `disk-buffer`: Added capacity, disk_allocated and disk_usage metrics.

      * "capacity_bytes": The theoretical maximal useful size of the disk-buffer.
                          This is always smaller, than `disk-buf-size()`, as there is some reserved
                          space for metadata. The actual full disk-buffer file can be larger than this,
                          as syslog-ng allows to write over this limit once, at the end of the file.

      * "disk_allocated_bytes": The current size of the disk-buffer file on the disk. Please note that
                                the disk-buffer file size does not strictly correlate with the number
                                of messages, as it is a ring buffer implementation, and also syslog-ng
                                optimizes the truncation of the file for performance reasons.

      * "disk_usage_bytes": The serialized size of the queued messages in the disk-buffer file. This counter
                            is useful for calculating the disk usage percentage (disk_usage_bytes / capacity_bytes)
                            or the remaining available space (capacity_bytes - disk_usage_bytes).

    Example metrics:
    ```
    syslogng_disk_queue_capacity_bytes{driver_id="d_network#0",driver_instance="tcp,localhost:1235",path="/var/syslog-ng-00000.rqf",reliable="true"} 104853504
    syslogng_disk_queue_disk_allocated_bytes{driver_id="d_network#0",driver_instance="tcp,localhost:1235",path="/var/syslog-ng-00000.rqf",reliable="true"} 17284
    syslogng_disk_queue_disk_usage_bytes{driver_id="d_network#0",driver_instance="tcp,localhost:1235",path="/var/syslog-ng-00000.rqf",reliable="true"} 13188
    ```
    ([#4356](https://github.com/syslog-ng/syslog-ng/pull/4356))

  * `kubernetes()`: Added `input_events_total` and `input_event_bytes_total` metrics.

    ```
    syslogng_input_events_total{cluster="k8s",driver="kubernetes",id="#anon-source0",namespace="default",pod="log-generator-1682517834-7797487dcc-49hqc"} 25
    syslogng_input_event_bytes_total{cluster="k8s",driver="kubernetes",id="#anon-source0",namespace="default",pod="log-generator-1682517834-7797487dcc-49hqc"} 1859
    ```
    ([#4447](https://github.com/syslog-ng/syslog-ng/pull/4447))

## Bugfixes

  * `pdbtool test`: fix two type validation bugs:

      1) When `pdbtool test` validates the type information associated with a name-value
         pair, it was using string comparisons, which didn't take type aliases
         into account. This is now fixed, so that "int", "integer" or "int64"
         can all be used to mean the same type.

      2) When type information is missing from a `<test_value/>` tag, don't
         validate it against "string", rather accept any extracted type.

    In addition to these fixes, a new alias "integer" was added to mean the same
    as "int", simply because syslog-ng was erroneously using this term when
    reporting type information in its own messages.
    ([#4405](https://github.com/syslog-ng/syslog-ng/pull/4405))

  * `$(format-json)`: fix RFC8259 number violation

    `$(format-json)` produced invalid JSON output when it contained numeric values with leading zeros or + signs.
    This has been fixed.
    ([#4415](https://github.com/syslog-ng/syslog-ng/pull/4415))

  * `grouping-by()`: fix `persist-name()` option not taken into account
    ([#4390](https://github.com/syslog-ng/syslog-ng/pull/4390))

  * `python()`, `db-parser()`, `grouping-by()`, `add-contextual-data()`: fix typing compatibility with <4.0 config versions
    ([#4394](https://github.com/syslog-ng/syslog-ng/pull/4394))

  * `python`: Fixed a crash which occurred at reloading after registering a confgen plugin.
    ([#4459](https://github.com/syslog-ng/syslog-ng/pull/4459))

  * `date-parser()`: fix `%z` when system timezone has no daylight saving time
    ([#4401](https://github.com/syslog-ng/syslog-ng/pull/4401))

  * Consider messages consumed into correlation states "matching": syslog-ng's
    correlation functionality (e.g.  grouping-by() or db-parser() with such
    rules) drop individual messages as they are consumed into a correlation
    contexts and you are using `inject-mode(aggregate-only)`.  This is usually
    happens because you are only interested in the combined message and not in
    those that make up the combination. However, if you are using correlation
    with conditional processing (e.g. if/elif/else or flags(final)), such
    messages were erroneously considered as unmatching, causing syslog-ng to
    take the alternative branch.

    Example:

    With a configuration similar to this, individual messages are consumed into
    a correlation state and dropped by `grouping-by()`:

    ```
    log {
        source(...);

        if {
            grouping-by(... inject-mode(aggregate-only));
        } else {
            # alternative branch
        };
    };
    ```

    The bug was that these individual messages also traverse the `else` branch,
    even though they were successfully processed with the inclusion into the
    correlation context. This is not correct. The bugfix changes this behaviour.
    ([#4370](https://github.com/syslog-ng/syslog-ng/pull/4370))

  * `netmask6()`: fix crash when user specifies too long mask
    ([#4429](https://github.com/syslog-ng/syslog-ng/pull/4429))

  * `afprog`: Fixed possible freezing on some OSes
    ([#4438](https://github.com/syslog-ng/syslog-ng/pull/4438))

  * `network()`, `syslog()`, `syslog-parser()`: fix null termination of SDATA param names
    ([#4429](https://github.com/syslog-ng/syslog-ng/pull/4429))

  * `python()`: fix LogMessage subscript not raising KeyError on non-existent keys

    When message fields were queried (`msg["key"]`) and the given key did not exist,
    `None` or an empty string was returned (depending on the version of the config).

    Neither was correct, now a KeyError occurs in such cases.
    ([#4410](https://github.com/syslog-ng/syslog-ng/pull/4410))

  * `$(python)`: fix template function prefix being overwritten when using datetime types
    ([#4410](https://github.com/syslog-ng/syslog-ng/pull/4410))

  * `disk-buffer`: Fixed queued messages stats counting, when a disk-buffer became corrupted.
    ([#4385](https://github.com/syslog-ng/syslog-ng/pull/4385))

  * `$(format-json)`: fix escaping control characters

    `$(format-json)` produced invalid JSON output when a string value contained control characters.
    ([#4417](https://github.com/syslog-ng/syslog-ng/pull/4417))

  * `disk-buffer()`: fix deinitialization when starting syslog-ng with invalid configuration
    ([#4418](https://github.com/syslog-ng/syslog-ng/pull/4418))

  * `python()`: fix exception handling when LogMessage value conversion fails
    ([#4410](https://github.com/syslog-ng/syslog-ng/pull/4410))

  * `json-parser()`: Fixed parsing non-string arrays.

    syslog-ng now no longer parses non-string arrays to list of strings, losing the original type
    information of the array's elements.
    ([#4396](https://github.com/syslog-ng/syslog-ng/pull/4396))

  * `disk-buffer`: Fixed a rare race condition when calculating disk-buffer filename.
    ([#4381](https://github.com/syslog-ng/syslog-ng/pull/4381))

  * `python-persist`: fix off-by-one overflow
    ([#4429](https://github.com/syslog-ng/syslog-ng/pull/4429))

## Packaging
  * The `--with-python-venv-dir=path` configure option can be used to modify the location of syslog-ng's venv.
    The default is still `${localstatedir}/python-venv`.
    ([#4465](https://github.com/syslog-ng/syslog-ng/pull/4465))


## Other changes

  * The `sdata-prefix()` option does not accept values longer than 128 characters.
    ([#4429](https://github.com/syslog-ng/syslog-ng/pull/4429))

  * `grouping-by()`: Remove setting of the `${.classifier.context_id}`
    name-value pair in all messages consumed into a correlation context.  This
    functionality is inherited from db-parser() and has never been documented
    for `grouping-by()`, has of limited use, and any uses can be replaced by the
    use of the built-in macro named `$CONTEXT_ID`.  Modifying all consumed
    messages this way has significant performance consequences for
    `grouping-by()` and removing it outweighs the small incompatibility this
    change introduces. The similar functionality in `db-parser()` correlation is
    not removed with this change.
    ([#4424](https://github.com/syslog-ng/syslog-ng/pull/4424))

  * `config`: Added `internal()` option to `source`s, `destination`s, `parser`s and `rewrite`s.

    Its main usage is in SCL blocks. Drivers configured with `internal(yes)` register
    their metrics on level 3. This makes developers of SCLs able to create metrics manually
    with `metrics-probe()` and "disable" every other metrics, they do not need.
    ([#4451](https://github.com/syslog-ng/syslog-ng/pull/4451))

  * The following Prometheus metrics have been renamed:

    `log_path_{in,e}gress` -> `route_{in,e}gress_total`
    `internal_source` -> `internal_events_total`

    The `internal_queue_length` stats counter has been removed.
    It was deprecated since syslog-ng 3.29.
    ([#4411](https://github.com/syslog-ng/syslog-ng/pull/4411))


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

Alex Becker, Attila Szakacs, Balazs Scheidler, Hofi, László Várady,
Muhammad Shanif, Ricfilipe, Romain Tartière
