4.0.0
=====

## Highlights

<Fill this block manually from the blocks below>

## Features

  * `$(format-json)`: fix a bug in the --key-delimiter option introduced in
    3.38, which causes the generated JSON to contain multiple values for the
    same key in case the key in question contains a nested object and
    key-delimiter specified is not the dot character.
    ([#4127](https://github.com/syslog-ng/syslog-ng/pull/4127))
  * `log-level()`: added a new global option to control syslog-ng's own internal
    log level.  This augments the existing support for doing the same via the
    command line (via -d, -v and -t options) and via syslog-ng-ctl.  This change
    also causes higher log-levels to include messages from lower log-levels,
    e.g.  "trace" also implies "debug" and "verbose".  By adding this capability
    to the configuration, it becomes easier to control logging in containerized
    environments where changing command line options is more challenging.

    `syslog-ng-ctl log-level`: this new subcommand in syslog-ng-ctl allows
    setting the log level in a more intuitive way, compared to the existing
    `syslog-ng-ctl verbose|debug|trace -s` syntax.

    `syslog-ng --log-level`: this new command line option for the syslog-ng
    main binary allows you to set the desired log-level similar to how you
    can control it from the configuration or through `syslog-ng-ctl`.
    ([#4091](https://github.com/syslog-ng/syslog-ng/pull/4091))
  * `network`/`syslog`/`http` destination: OCSP stapling support

    OCSP stapling support for network destinations and for the `http()` module has been added.

    When OCSP stapling verification is enabled, the server will be requested to send back OCSP status responses.
    This status response will be verified using the trust store configured by the user (`ca-file()`, `ca-dir()`, `pkcs12-file()`).

    Note: RFC 6961 multi-stapling and TLS 1.3-provided multiple responses are currently not validated, only the peer certificate is verified.

    Example config:
    ```
    destination {

        network("test.tld" transport(tls)
            tls(
                pkcs12-file("/path/to/test.p12")
                peer-verify(yes)
                ocsp-stapling-verify(yes)
            )
        );

        http(url("https://test.tld") method("POST") tls(peer-verify(yes) ocsp-stapling-verify(yes)));
    };
    ```
    ([#4082](https://github.com/syslog-ng/syslog-ng/pull/4082))

## Bugfixes

  * afsocket-dest: fixed a crash when a kept-alive connection after reload which was not initalized properly (e.g. due to name resolution issues of the remote hostname) receives a connection close from the remote.
    ([#4044](https://github.com/syslog-ng/syslog-ng/pull/4044))
  * `grouping-by()` persist-name() option: fixed a segmentation fault in the
    grammar.
    ([#4180](https://github.com/syslog-ng/syslog-ng/pull/4180))
  * `add-contextual-data()`: add compatibility warnings and update advise in
    case of the value field of the add-contextual-data() database contains an
    expression that resembles the new type-hinting syntax: type(value).

    `db-parser()`: add compatibility warnings and update advise in case the
    <value>  element in the pattern database contains an expression that
    resembles the new type-hinting syntax: type(value).
    ([#4158](https://github.com/syslog-ng/syslog-ng/pull/4158))
  * `syslog-ng --help` screen: the output for the --help command line option has
    included sample paths to various files that contained autoconf style
    directory references (e.g. ${prefix}/etc for instance). This is now fixed,
    these paths will contain the expanded path. Fixes Debian Bug report #962839:
    https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=962839
    ([#4143](https://github.com/syslog-ng/syslog-ng/pull/4143))
  * `python`: Fixed a crash, when trying to get "R_STAMP", "P_STAMP" or "STAMP" from the log message.
    ([#4057](https://github.com/syslog-ng/syslog-ng/pull/4057))

## Packaging

  * `python`: python2 support is now completely removed. `syslog-ng` can no longer be configured with `--with-python=2`.
    ([#4057](https://github.com/syslog-ng/syslog-ng/pull/4057))
  * `python`: Python 2 support is now completely removed from Light too. Light will support only Python 3.
    ([#4174](https://github.com/syslog-ng/syslog-ng/pull/4174))

## Other changes

  * `sumologic-http()` improvements

    Breaking change: `sumologic-http()` originally sent incomplete messages (only the `$MESSAGE` part) to Sumo Logic by default.
    The new default is a JSON object, containing all name-value pairs.

    To override the new message format, the `template()` option can be used.

    `sumologic-http()` now supports batching, which is enabled by default to increase the destination's performance.

    The `tls()` became optional, Sumo Logic servers will be verified using the system's certificate store by default.
    ([#4124](https://github.com/syslog-ng/syslog-ng/pull/4124))
  * docker image: Nightly production docker images are now available as `balabit/syslog-ng:nightly`
    ([#4117](https://github.com/syslog-ng/syslog-ng/pull/4117))
  * `make dist`: fixed make dist of FreeBSD so that source tarballs can
    easily be produced even if running on FreeBSD.
    ([#4163](https://github.com/syslog-ng/syslog-ng/pull/4163))
  * `dbld`: Removed support for `ubuntu-xenial`.
    ([#4057](https://github.com/syslog-ng/syslog-ng/pull/4057))
  * `dbld`: The `syslog-ng-mod-python` package is now built with `python3` on the following platforms:
      * `debian-stretch`
      * `debian-buster`
      * `ubuntu-bionic`
    ([#4057](https://github.com/syslog-ng/syslog-ng/pull/4057))

## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Andras Mitzki, Attila Szakacs, Attila Szalay, Balazs Scheidler,
Bálint Horváth, Gabor Nagy, Joshua Root, László Várady, Szilárd Parrag
