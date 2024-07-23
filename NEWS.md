4.8.0
=====

## Highlights

### Default config version in configuration files

`cfg`: allow usage of `current` in config @version by default if it is not presented

This change allows syslog-ng to start even if the `@version` information is not present in the configuration file and treats the version as the latest in that case.

**NOTE:** syslog-ng will still raise a warning if `@version` is not present. Please use `@version: current` to confirm the intention of always using the latest version and silence the warning. ([#5030](https://github.com/syslog-ng/syslog-ng/pull/5030))

### BSD directory monitoring with kqueue

`directory-monitor`: Added a kqueue based directory monitor implementation.

`wildcard-file()` sources are using a [directory monitor](https://syslog-ng.github.io/admin-guide/060_Sources/020_File/001_File_following) as well to aid detection of changes in the folders of the followed files. The new kqueue-based directory monitor uses far fewer resources than the `poll` based version on BSD-based systems.
([#5022](https://github.com/syslog-ng/syslog-ng/pull/5022))

See more at the [new syslog-ng documentation](https://syslog-ng.github.io/admin-guide/060_Sources/020_File/001_File_following).

### Wildcard file source fine-tuning

`wildcard-file()`: Added a dedicated `monitor_freq` option to control the poll frequency of the change detection in the directories separately when the `poll` method is selected via the `monitor-method()` option.

The `monitor-method()` option controls only the change detection method in the directories, not the following of the file changes, and if `poll` is the selected method the frequency must not necessarily be the same, e.g. if the (earlier) commonly used `follow-freq()` is set to 0 for switching to the `poll_fd_events` method for file content change detection, that also might be meant a directory change poll with zero delays (if `monitor-method()` was set to `poll` as well), and that could cause a heavy CPU load unnecessarily.
([#4998](https://github.com/syslog-ng/syslog-ng/pull/4998))

See more at the [new syslog-ng documentation](https://syslog-ng.github.io/admin-guide/060_Sources/020_File/001_File_following).

## Features

  * `s3()`: Introduced server side encryption related options

    `server-side-encryption()` and `kms-key()` can be used to configure encryption.

    Currently only `server-side-encryption("aws:kms")` is supported.
    The `kms-key()` should be:
      * an ID of a key
      * an alias of a key, but in that case you have to add the alias/prefix
      * an ARN of a key

    To be able to use the aws:kms encryption the AWS Role or User has to have the following
    permissions on the given key:
      * `kms:Decrypt`
      * `kms:Encrypt`
      * `kms:GenerateDataKey`

    Check [this](https://repost.aws/knowledge-center/s3-large-file-encryption-kms-key) page on why the `kms:Decrypt` is mandatory.

    Example config:
    ```
    destination d_s3 {
      s3(
        bucket("log-archive-bucket")
        object-key("logs/syslog")
        server-side-encryption("aws:kms")
        kms-key("alias/log-archive")
      );
    };
    ```

    See the [S3](https://docs.aws.amazon.com/AmazonS3/latest/userguide/UsingKMSEncryption.html) documentation for more details.
    ([#4993](https://github.com/syslog-ng/syslog-ng/pull/4993))

  * `filter`: Added numerical severity settings.

    The `level` filter option now accepts numerical values similar to `facility`.

    Example config:
    ```
    filter f_severity {
      level(4)
    };
    ```
    This is equivalent to
    ```
    filter f_severity {
      level("warning")
    };
    ```

    For more information, consult the [documentation](https://syslog-ng.github.io/admin-guide/080_Log/030_Filters/005_Filter_functions/004_level_priority.html).
    ([#5016](https://github.com/syslog-ng/syslog-ng/pull/5016))

  * `opentelemetry()`, `loki()`, `bigquery()`: Added `headers()` option

    Enables adding headers to RPC calls.

    Example config:
    ```
    opentelemetry(
      ...
      headers(
        "my_header" = "my_value"
      )
    );
    ```
    ([#5012](https://github.com/syslog-ng/syslog-ng/pull/5012))

  * #### Added new proxy options to the `syslog()` and `network()` source drivers

    The `transport(proxied-tcp)`, `transport(proxied-tls)`, and `transport(proxied-tls-passthrough)` options are now available when configuring `syslog()` and `network()` sources.
    ([#4544](https://github.com/syslog-ng/syslog-ng/pull/4544))


## Bugfixes

  * `disk-buffer()`: fix crash when pipeline initialization fails

    `log_queue_disk_free_method: assertion failed: (!qdisk_started(self->qdisk))`
    ([#4994](https://github.com/syslog-ng/syslog-ng/pull/4994))

  * `rate-limit()`: Fixed a crash which occured on a config parse failure.
    ([#5033](https://github.com/syslog-ng/syslog-ng/pull/5033))

  * Fixed potential null pointer deref issues
    ([#5035](https://github.com/syslog-ng/syslog-ng/pull/5035))

  * `wildcard-file()`: fix a crash and detection of file delete/move when using ivykis poll events

    Two issues were fixed

    - Fixed a crash in log pipe queue during file deletion and EOF detection (#4989)

       The crash was caused by a concurrency issue in the EOF and file deletion detection when using a `wildcard-file()` source.

       If a file is written after being deleted (e.g. with an application keeping the file open), or if these events happen concurrently, the file state change poller mechanism might schedule another read cycle even though the file has already been marked as fully read and deleted.

       To prevent this re-scheduling between these two checks, the following changes have been made:
       Instead of maintaining an internal EOF state in the `WildcardFileReader`, when a file deletion notification is received, the poller will be signaled to stop after reaching the next EOF. Only after both conditions are set the reader instance will be deleted.

    - Fixed the file deletion and removal detection when the `file-reader` uses `poll_fd_events` to follow file changes, which were mishandled. For example, files that were moved or deleted (such as those rolled by a log-rotator) were read to the end but never read again if they were not touched anymore, therefore switching to the new file never happened.
    ([#4998](https://github.com/syslog-ng/syslog-ng/pull/4998))

  * `syslog-ng-ctl query`: fix showing Prometheus metrics as unnamed values

    `none.value=726685`
    ([#4995](https://github.com/syslog-ng/syslog-ng/pull/4995))

  * macros: Fixed a bug which always set certain macros to string type

    The affected macros are `$PROGRAM`, `$HOST` and `$MESSAGE`.
    ([#5024](https://github.com/syslog-ng/syslog-ng/pull/5024))
 
  * `syslog-ng-ctl query`: show timestamps and fix `g_pattern_spec_match_string` assert
    ([#4995](https://github.com/syslog-ng/syslog-ng/pull/4995))

  * `csv-parser()`: fix escape-backslash-with-sequences dialect on ARM

    `csv-parser()` produced invalid output on platforms where char is an unsigned type.
    ([#4947](https://github.com/syslog-ng/syslog-ng/pull/4947))


## Other changes

  * `bigquery()`, `loki()`, `opentelemetry()`, `cloud-auth()`: C++ modules can be compiled with clang

    Compiling and using these C++ modules are now easier on FreeBSD and macOS.
    ([#4933](https://github.com/syslog-ng/syslog-ng/pull/4933))

  * `syslog-ng-ctl`: do not show orphan metrics for `stats prometheus`

    As the `stats prometheus` command is intended to be used to forward metrics
    to Prometheus or any other time-series database, displaying orphaned metrics
    should be avoided in order not to insert new data points when a given metric
    is no longer alive.

    In case you are interested in the last known value of orphaned counters, use
    the `stats` or `query` subcommands.
    ([#4921](https://github.com/syslog-ng/syslog-ng/pull/4921))

  * `s3()`: new metric `syslogng_output_event_bytes_total`
    ([#4958](https://github.com/syslog-ng/syslog-ng/pull/4958))

  * multiline-options: Allow `multi_line_timeout` to be set to a non-integer value.

    Since `multi_line_timeout` is suggested to be set as a multiple of `follow-freq`, and `follow-freq` can be much smaller than one second, it makes sense to allow this value to be a non-integer as well.
    ([#5002](https://github.com/syslog-ng/syslog-ng/pull/5002))

  * packages/dbld: add support for Ubuntu 24.04 (Noble Numbat)
    ([#4925](https://github.com/syslog-ng/syslog-ng/pull/4925))

  * packages/dbld: add support for AlmaLinux 9
    ([#5009](https://github.com/syslog-ng/syslog-ng/pull/5009))

  * packages/dbld: added support for Fedora Rawhide and CentOS Stream 9 as testing platforms
    ([#5009](https://github.com/syslog-ng/syslog-ng/pull/5009))


## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Alex Becker, Andras Mitzki, Arpad Kunszt, Attila Szakacs,
Balazs Scheidler, Bálint Horváth, Dmitry Levin, Hofi, Ilya Kheifets,
joohoonmaeng, ktzsolt, László Várady, Mate Ory, Natanael Copa,
Peter Czanik, qsunchiu, Robert Fekete, shifter, Szilárd Parrag,
Tamas Pal, Wolfram Joost
