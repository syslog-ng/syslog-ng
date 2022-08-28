3.38.1
======

## Highlights

### Sneak peek into syslog-ng v4.0

syslog-ng v4.0 is right around the corner.

This release (v3.38.1) contains all major changes, however, they are
currently all hidden behind a feature flag.
To enable and try those features, you need to specify `@version: 4.0` at the
top of the configuration file.

You can find out more about the 4.0 changes and features
[here](https://github.com/syslog-ng/syslog-ng/blob/master/NEWS-4.0.md).

Read our practical introduction to typing at
[syslog-ng-future.blog](https://syslog-ng-future.blog/syslog-ng-4-progress-3-38-1-release/).

## Features

  * `grouping-by()`: added `inject-mode(aggregate-only)`

    This inject mode will drop individual messages that make up the correlation
    context (`key()` groups) and would only yield the aggregate messages
    (e.g. the results of the correlation).
    ([#3998](https://github.com/syslog-ng/syslog-ng/pull/3998))
  * `add-contextual-data()`: add support for type propagation, e.g. set the
    type of name-value pairs as they are created/updated to the value returned
    by the template expression that we use to set the value.

    The 3rd column in the CSV file (e.g. the template expression) now supports
    specifying a type-hint, in the format of "type-hint(template-expr)".

    Example line in the CSV database:

    selector-value,name-value-pair-to-be-created,list(foo,bar,baz)
    ([#4051](https://github.com/syslog-ng/syslog-ng/pull/4051))
  * `$(format-json)`: add --key-delimiter option to reconstruct JSON objects
    using an alternative structure separator, that was created using the
    key-delimiter() option of json-parser().
    ([#4093](https://github.com/syslog-ng/syslog-ng/pull/4093))
  * `json-parser()`: add key-delimiter() option to extract JSON structure
    members into name-value pairs, so that the names are flattened using the
    character specified, instead of dot.

    Example:
      Input: {"foo":{"key":"value"}}

      Using json-parser() without key-delimiter() this is extracted to:

          foo.key="value"

      Using json-parser(key-delimiter("~")) this is extracted to:

          foo~key="value"

    This feature is useful in case the JSON keys contain dots themselves, in
    those cases the syslog-ng representation is ambigious.
    ([#4093](https://github.com/syslog-ng/syslog-ng/pull/4093))

## Bugfixes

  * Fixed buffer handling of syslog and timestamp parsers

    Multiple buffer out-of-bounds issues have been fixed, which could cause
    hangs, high CPU usage, or other undefined behavior.
    ([#4110](https://github.com/syslog-ng/syslog-ng/pull/4110))
  * Fixed building with LibreSSL
    ([#4081](https://github.com/syslog-ng/syslog-ng/pull/4081))
  * `network()`: Fixed a bug, where syslog-ng halted the input instead of skipping a character
    in case of a character conversion error.
    ([#4084](https://github.com/syslog-ng/syslog-ng/pull/4084))
  * `redis()`: Fixed bug where using redis driver without the `batch-lines` option caused program crash.
    ([#4114](https://github.com/syslog-ng/syslog-ng/pull/4114))
  * `pdbtool`: fix a SIGABRT on FreeBSD that was triggered right before pdbtool
    exits. Apart from being an ugly crash that produces a core file,
    functionally the tool behaved correctly and this case does not affect
    syslog-ng itself.
    ([#4037](https://github.com/syslog-ng/syslog-ng/pull/4037))
  * `regexp-parser()`: due to a change introduced in 3.37, named capture groups
    are stored indirectly in the LogMessage to avoid copying of the value.  In
    this case the name-value pair created with the regexp is only stored as a
    reference (name + length of the original value), which improves performance
    and makes such name-value pairs use less memory.  One omission in the
    original change in 3.37 is that syslog-ng does not allow builtin values to
    be stored indirectly (e.g.  $MESSAGE and a few of others) and this case
    causes an assertion to fail and syslog-ng to crash with a SIGABRT. This
    abort is now fixed. Here's a sample config that reproduces the issue:

        regexp-parser(patterns('(?<MESSAGE>.*)'));
    ([#4043](https://github.com/syslog-ng/syslog-ng/pull/4043))
  * set-tag: fix cloning issue when string literal were used (see #4062)
    ([#4065](https://github.com/syslog-ng/syslog-ng/pull/4065))
  * `add-contextual-data()`: fix high memory usage when using large CSV files
    ([#4067](https://github.com/syslog-ng/syslog-ng/pull/4067))

## Other changes

  * The `json-c` library is no longer bundled in the syslog-ng source tarball

    Since all known OS package managers provide json-c packages nowadays, the json-c
    submodule has been removed from the source tarball.

    The `--with-jsonc=internal` option of the `configure` script has been removed
    accordingly, system libraries will be used instead. For special cases, the JSON
    support can be disabled by specifying `--with-jsonc=no`.
    ([#4078](https://github.com/syslog-ng/syslog-ng/pull/4078))
  * platforms: Dropped support for ubuntu-impish as it became EOL
    ([#4088](https://github.com/syslog-ng/syslog-ng/pull/4088))

## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Alvin Šipraga, Andras Mitzki, Attila Szakacs, Balazs Scheidler,
Bálint Horváth, Daniel Klauer, Fabrice Fontaine, Gabor Nagy,
HenryTheSir, László Várady, Parrag Szilárd, Peter Kokai, Shikhar Vashistha,
Szilárd Parrag, Vivin Peris
