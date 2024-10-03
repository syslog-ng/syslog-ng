4.8.1
=====

## Highlights

 * `elasticsearch-datastream()` destinations can be used to feed Elasticsearch [data streams](https://www.elastic.co/guide/en/elasticsearch/reference/current/data-streams.html).

    Example config:

    ```
    elasticsearch-datastream(
      url("https://elastic-endpoint:9200/my-data-stream/_bulk")
      user("elastic")
      password("ba3DI8u5qX61We7EP748V8RZ")
    );
    ```
    ([#5069](https://github.com/syslog-ng/syslog-ng/pull/5069))
   
 * `building`: thanks to Sergey Fedorov (@barracuda156) and Marius Schamschula (@Schamschula), macOS builds now support gcc again. They also updated the [MacPort version](https://github.com/macports/macports-ports/blob/30c55ba04d8c0693c18cdf84014187cd3c53e60f/sysutils/syslog-ng-devel/Portfile) of syslog-ng (develop). Great work, and thank you so much for your contribution!
    ([#5108](https://github.com/syslog-ng/syslog-ng/pull/5108))
   
## Features

  * `tls()`: expose the key fingerprint of the peer in `${.tls.x509_fp}` if
    `trusted-keys()` is used to retain the actual peer identity in the received
    messages.
    ([#5068](https://github.com/syslog-ng/syslog-ng/pull/5068))

  * `syslog-parser`: Added the `no-piggyback-errors` and the `piggyback-errors` flags to control whether the message retains the original message or not on parse error(s). By default the old behaviour/`piggyback-errors` flag is active.

    - `no-piggyback-errors`: On failure, the original message will be left as it was before parsing, the value of `$MSGFORMAT` will be set to `syslog-error`, and a tag will be placed on the message corresponding to the parser's failure.
    - `piggyback-errors`: On failure, the old behaviour is used (clearing the entire message then syslog-ng will generate a new message in place of the old one describing the parser's error).

    The following new tags can be added by the `syslog-parser` to the message when the parsing failed:
      - `syslog.rfc5424_missing_hostname`
      - `syslog.rfc5424_missing_app_name`
      - `syslog.rfc5424_missing_procid`
      - `syslog.rfc5424_missing_msgid`
      - `syslog.rfc5424_missing_sdata`
      - `syslog.rfc5424_invalid_sdata`
      - `syslog.rfc5424_missing_message`
    ([#5063](https://github.com/syslog-ng/syslog-ng/pull/5063))


## Bugfixes

  * `syslog-ng-ctl`: fix escaping of `stats prometheus`

    Metric labels (for example, the ones produced by `metrics-probe()`) may contain control characters, invalid UTF-8 or `\`
    characters. In those specific rare cases, the escaping of the `stats prometheus` output was incorrect.
    ([#5046](https://github.com/syslog-ng/syslog-ng/pull/5046))

  * `wildcard-file()`: fix crashes can occure if the same wildcard file is used in multiple sources

    Because of some persistent name construction and validation bugs the following config crashed `syslog-ng`
    (if there were more than one log file is in the `/path` folder)

    ``` config
    @version: current

    @include "scl.conf"

    source s_files1 {
        file("/path/*.log"
            persist-name("p1")
        );
    };

    source s_files2 {
        file("/path/*.log"
            persist-name("p2")
        );
    };

    destination s_stdout {
        stdout();
    };

    log {
        source(s_files1);
        destination(s_stdout);
    };

    log {
        source(s_files2);
        destination(s_stdout);
    };
    ```

    NOTE:

    - The issue occurred regardless of the presence of the `persist-name()` option.
    - It affected not only the simplified example of the legacy wildcard `file()` but also the new `wildcard-file()` source.
    ([#5091](https://github.com/syslog-ng/syslog-ng/pull/5091))

  * `syslog-ng-ctl`: fix crash of syslog-ng service in g_hash_table lookup function after `syslog-ng-ctl reload`
    ([#5087](https://github.com/syslog-ng/syslog-ng/pull/5087))

  * `file()`, `stdout()`: fix log sources getting stuck

    Due to an acknowledgment bug in the `file()` and `stdout()` destinations,
    sources routed to those destinations may have gotten stuck as they were
    flow-controlled incorrectly.

    This issue occured only in extremely rare cases with regular files, but it
    occured frequently with `/dev/stderr` and other slow pseudo-devices.
    ([#5134](https://github.com/syslog-ng/syslog-ng/pull/5134))

  * `directory-monitor`: fixed a main thread assertion crash that might have occurred during syslog-ng stop or restart
    ([#5086](https://github.com/syslog-ng/syslog-ng/pull/5086))

  * `Config  @version`: fixed compat-mode inconsistencies when `@version` was not specified at the top of the configuration
    file or was not specified at all
    ([#5145](https://github.com/syslog-ng/syslog-ng/pull/5145))

  * `grpc`: Fix potential memoryleak when the grpc module is loaded but not used.
    ([#5062](https://github.com/syslog-ng/syslog-ng/pull/5062))

  * `s3()`: Eliminated indefinite memory usage increase for each reload.

    The increased memory usage is caused by the `botocore` library, which
    caches the session information. We only need the Session object, if
    `role()` is set. The increased memory usage still happens with that set,
    currently we only fixed the unset case.
    ([#5149](https://github.com/syslog-ng/syslog-ng/pull/5149))

  * `opentelemetry()` sources: fix crash when `workers()` is set to `> 1`
    ([#5138](https://github.com/syslog-ng/syslog-ng/pull/5138))

  * 
    `opentelemetry()` sources: fix source hang-up on flow-controlled paths
    ([#5148](https://github.com/syslog-ng/syslog-ng/pull/5148))

  * `metrics-probe()`: fix disappearing metrics from `stats prometheus` output

    `metrics-probe()` metrics became orphaned and disappeared from the `syslog-ng-ctl stats prometheus` output
    whenever an ivykis worker stopped (after 10 seconds of inactivity).
    ([#5075](https://github.com/syslog-ng/syslog-ng/pull/5075))

  * `affile`: Fix an invalid `lseek` call mainly on the `pipe()` source, but also possible if using affile on pipe like files (pipe, socket and FIFO).
    ([#5058](https://github.com/syslog-ng/syslog-ng/pull/5058))


## Other changes

  * `format-json`: spaces around `=` in `$(format-json)` template function could cause a
    [crash](https://github.com/syslog-ng/syslog-ng/issues/5065).
    The fix of the issue also introduced an enhancement, from now on spaces are allowed
    around the `=` operator, so the following `$(format-json)` template function calls
    are all valid:
    ```
    $(format-json foo =alma)
    $(format-json foo= alma)
    $(format-json foo = alma)
    $(format-json foo=\" alma \")
    $(format-json foo= \" alma \")
    $(format-json foo1= alma foo2 =korte foo3 = szilva foo4 = \" meggy \" foo5=\"\")
    ```
    Please note the usage of the escaped strings like `\" meggy \"`, and the (escaped and) quoted form
    that used for an empty value `\"\"`, the latter is a breaking change as earlier an expression like
    `key= ` led to a json key-value pair with an empty value `{"key":""}` that will not work anymore.
    ([#5080](https://github.com/syslog-ng/syslog-ng/pull/5080))

  * `building`: fixed multiple potentional FreeBSD build errors
    ([#5099](https://github.com/syslog-ng/syslog-ng/pull/5099))

  * `docker`: Changed the container image's base to debian:bookworm.
    ([#5056](https://github.com/syslog-ng/syslog-ng/pull/5056))


## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Andras Mitzki, Attila Szakacs, Balazs Scheidler, Hofi,
Kovács Gergő Ferenc, László Várady, Mate Ory,
Peter Czanik (CzP), Sergey Fedorov, Marius Schamschula, Szilard Parrag,
Tamas Pal, shifter
