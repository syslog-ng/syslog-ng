3.74.74
=======

## Highlights

<Fill this block manually from the blocks below>

## Features

  * `set-matches()` and `unset-matches()`: these new rewrite operations allow
    the setting of match variables ($1, $2, ...) in a single operation, based
    on a syslog-ng list expression.

    Example:
      # set $1, $2 and $3 respectively
      set-matches("foo,bar,baz")

      # likewise, but using a list function
      set-matches("$(explode ':' 'foo:bar:baz')");
    ([#3948](https://github.com/syslog-ng/syslog-ng/pull/3948))
  * `syslog-parser()`: allow comma (e.g. ',') to separate the seconds and the fraction of a
    second part as some devices use that character. This change applies to both
    to syslog-parser() and the builtin syslog parsing functionality of network
    source drivers (e.g. udp(), tcp(), network() and syslog()).
    ([#3949](https://github.com/syslog-ng/syslog-ng/pull/3949))
  * `internal()`: add rcptid tag to all trace messages that relate to incoming
    log messages.  This makes it easier to correlate parsing, rewriting and
    routing actions with incoming log messages.
    ([#3972](https://github.com/syslog-ng/syslog-ng/pull/3972))
  * `dbld`: add support for Fedora 35 in favour of Fedora 33
    ([#3933](https://github.com/syslog-ng/syslog-ng/pull/3933))
  * `cisco-parser`: add ISO 8601 timestamp support
    ([#3934](https://github.com/syslog-ng/syslog-ng/pull/3934))
  * `mariadb-audit-parser`: A new parser for mariadb/mysql audit plugin logs have been added. The parser supports the `syslog` output type's format, see [mariadb page](https://mariadb.com/kb/en/mariadb-audit-plugin) for details.
    ([#3947](https://github.com/syslog-ng/syslog-ng/pull/3947))
  * `$*` macro: the $* macro in template expressions convert the match variables
    (e.g. $1, $2, ...) into a syslog-ng list that can be further manipulated
    using the list template functions, or turned into a list in type-aware
    destinations.
    ([#3948](https://github.com/syslog-ng/syslog-ng/pull/3948))
  * `network()`, `syslog()` sources and destinations: added new TLS options `sigalgs()` and `client-sigalgs()`

    They can be used to restrict which signature/hash pairs can be used in digital signatures.
    It sets the "signature_algorithms" extension specified in RFC5246 and RFC8446.

    Example configuration:

    ```
    destination {
        network("test.host" port(4444) transport(tls)
            tls(
                pkcs12-file("/home/anno/Work/syslog-ng/tls/test.host.p12")
                peer-verify(yes)
                sigalgs("RSA-PSS+SHA256:ed25519")
            )
        );
    };
    ```
    ([#4000](https://github.com/syslog-ng/syslog-ng/pull/4000))

## Bugfixes

  * `grouping-by()`: fix `grouping-by()` use through parser references.
    Originally if a grouping-by() was part of a named parser statement and was
    referenced from multiple log statements, only the first grouping-by()
    instance behaved properly, 2nd and subsequent references were ignoring all
    configuration options and have reverted using defaults instead.

    `db-parser()`: similarly to `grouping-by()`, `db-parser()` also had issues
    propagating some of its options to 2nd and subsequent references of a parser
    statement. This includes `drop-unmatched()`, `program-template()` and
    `template()` options.
    ([#3957](https://github.com/syslog-ng/syslog-ng/pull/3957))
  * `match(), subst() and regexp-parser()`: fixed storing of numbered (e.g.  $1,
    $2, $3 and so on) and named capture groups in regular expressions in case
    the input of the regexp is the same as one of the match variables being
    stored.  In some cases the output of the regexp was clobbered and an invalid
    value stored.
    ([#3948](https://github.com/syslog-ng/syslog-ng/pull/3948))
  * `http()` and other threaded destinations: fix $SEQNUM processing so that
    only local messages get an associated $SEQNUM, just like normal
    syslog()-like destinations.  This avoids a [meta sequenceId="XXX"] SD-PARAM
    being added to $SDATA for non-local messages.
    ([#3928](https://github.com/syslog-ng/syslog-ng/pull/3928))
  * fix `threaded(no)` related crash: if threaded mode is disabled for
    asynchronous sources and destinations (all syslog-like drivers such as
    tcp/udp/syslog/network qualify), a use-after-free condition can happen due
    to a reference counting bug in the non-threaded code path.  The
    `threaded(yes)` setting has been the default since 3.6.1 so if you are using
    a more recent version, you are most probably unaffected.  If you are using
    `threaded(no)` a use-after-free condition happens as the connection closes.
    The problem is more likely to surface on 32 bit platforms due to pointer
    sizes and struct layouts where this causes a NULL pointer dereference.
    ([#3997](https://github.com/syslog-ng/syslog-ng/pull/3997))

## Packaging

  * debian: fix logrotate file not doing the file rotation. (The path and command was invalid.)
    ([#4031](https://github.com/syslog-ng/syslog-ng/pull/4031))

## Other changes

  * `java()/python() destinations`: the $SEQNUM macro (and "seqnum" attribute in
    Python) was erroneously for both local and non-local logs, while it should
    have had a value only in case of local logs to match RFC5424 behavior
    (section 7.3.1).  This bug is now fixed, but that means that all non-local
    logs will have $SEQNUM set to zero from this version on, e.g.  the $SEQNUM
    macro would expand to an string, to match the syslog() driver behaviour.
    ([#3928](https://github.com/syslog-ng/syslog-ng/pull/3928))

## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

, Andras Mitzki, Attila Szakacs, Balazs Scheidler, Gabor Nagy,
László Várady, Parrag Szilárd, Peter Kokai, Zoltan Pallagi
