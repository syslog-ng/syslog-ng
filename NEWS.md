3.6.0rc2
========
<!-- Tue, 14 Oct 2014 12:43:39 +0200 -->

This is the second (hopefully last) Release Candidate of the syslog-ng
OSE 3.6 branch. Some release critical bugs found and fixed.

Bugfixes
--------

* Opening control socket disabled when syslog-ng is used for only
  syntax-checking.
* Reloading a config file containing runtime error now not ends in a crash,
  it is able to fallback to the original config.
  (runtime error: config file is grammatically valid but containing invalid
   value, eg.: wrong database column name)
* Casting error eliminated in Riemann destination when metric is applied to
  an empty field.
* From now, syslog-ng always exclude attributes that conflict with properties
  in Riemann destination (otherwise value of the attribute would override the
  property).
* Patterndb fixed to apply condition even if context-id is missing.

Credits
-------

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessary to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Andras Mitzki, Balazs Scheidler, Fabien Wernli, Gergely Nagy, Laszlo Budai,
Viktor Tusa

3.6.0rc1
========
<!-- Fri, 26 Sep 2014 11:17:31 +0200 -->

This is the first Release Candidate of the syslog-ng OSE 3.6 branch.
Based on our test results this release is almost production ready.

Features
--------
* Added groupset rewrite object.
  Groupset allows the user to modify multiple log message properties at once.
  It also allows referencing the old value of the property as the $_ macro.

Bugfixes
--------
* Fixed a memory leak during configuration parsing when using `rewrite()`.
* Change control socket message from notice to debug
* Fixes for retries() functionality.
  Retry counter incremented by every message write error
  (including network connection errors) which can lead to message lost.

Credits
-------

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessary to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Balazs Scheidler, Brian De Wolf, Gergely Nagy, Laszlo Budai, Peter Czanik,
Tibor Benke, Viktor Juhasz, Viktor Tusa

3.6.0beta2
==========
<!-- Thu, 11 Sep 2014 13:09:57 +0200 -->

This is the second beta release of the upcoming syslog-ng OSE 3.6
branch. Compared to the previous beta, this release contains a few
minor features and bugfixes. We expect the next release to be a
release candidate, focusing on stability and bugfixes. Testing is most
appreciated!

Features
--------

* It is now possible to load not only a single certificate when using
  TLS, but a certificate chain.
* The `system()` source will not include `/dev/kmsg` (or `/proc/kmsg`)
  when running inside a Linux container.

Bugfixes
--------

* The `in-list()` filter was fixed to look at all elements of the
  list, instead of only the last one.
* Fixed an assertion when using the `match()` filter under certain
  circumstances.
* The `system()` source will not add `/dev/kmsg` (or `/proc/kmsg` on
  older kernels) to the default sources if using the systemd journal,
  because kernel logs are included in the journal.

Credits
-------

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessary to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Andras Mitzki, Balazs Scheidler, Gergely Nagy, Gyorgy Pasztor, Peter
Czanik, Tibor Benke.

3.6.0beta1
==========
<!-- Wed,  3 Sep 2014 14:40:14 +0200 -->

This is the first beta release of the upcoming syslog-ng OSE 3.6
branch. Compared to the alphas, this release contains a moderate
amount of new functionality and bugfixes. Further releases will focus
on stability and bugfixes.

Features
--------
* One can now use multiple elements in the `key()` and `exclude()`
  options of any value-pairs declaration.

* A new source driver was added to the syslog-ng: `systemd-journal()`,
  which reads from the Journal directly, not via the syslog forwarding
  socket. The `system()` source defaults to using this source when
  systemd is detected.

Bugfixes
--------

* All the various crypto-related template functions now check that the
  desired length of the digest is not larger than the digest itself.
  If a larger value is requested, they will truncate it to the digest
  length.

* The `$(geoip)` template function now works with `threaded(yes)` too.

* The unix domain socket credentials code was changed to only build on
  Linux and FreeBSD. With this change, syslog-ng should compile again
  on platforms where the OS does not support this, with the feature
  disabled.

Credits
-------

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessary to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Andras Mitzki, Fabien Wernli, Gergely Nagy, Laszlo Budai, Michael
Hocke, Tibor Benke, Viktor Juhasz, Viktor Tusa.

3.6.0alpha3
===========
<!-- Released: Tue, 19 Aug 2014 14:33:00 +0200 -->

This is the third alpha release of the upcoming syslog-ng OSE 3.6
branch. It is expected to be the last alpha release, with the first
beta in about two weeks. This release contains a number of important
features and bugfixes:

Changed defaults
----------------

* The `flush-lines()` setting now defaults to *100*, rather than *1*,
  for increased speed.

Features
--------

* The `system()` source will now parse `@cim` marked messages as JSON,
  if the JSON module is available at run-time. This improves
  inter-operation with other software that uses the *Common
  Information Model*.

### Features from the [Incubator][incubator]

* The `$(or)` template function that returns the first non-empty
  argument is now included in syslog-ng itself.

* The `$(padding)` template function, to pad text with custom padding
  to a given length is also included.

* The `$(graphite-output)` template function, to be used for sending
  metrics to [Graphite][graphite] was ported over from the Incubator.
  The `graphite()` destination SCL block is also available now, to
  make it even easier to talk to Graphite.

* The `riemann()` destination, which allows sending metrics to the
  [Riemann][riemann] monitoring system was also ported over from the
  Incubator.

 [graphite]: http://graphite.wikidot.com/
 [incubator]: https://github.com/balabit/syslog-ng-incubator
 [riemann]: http://riemann.io/

### Threaded destinations

A number of features were implemented for all threaded destinations:
`amqp()`, `mongodb()`, `redis()`, `riemann()`, `smtp()` and `stomp()`.

* The destinations gained support for `SEQNUM` persistence: the
  counter will be preserved across reloads and restarts.

* A new option called `retries()` was implemented for all of these,
  which controls how many times a message delivery is retried before
  dropping it.

* The `throttle()` option is now implemented, and works for all of the
  aforementioned destination drivers.

* The message delivery loop was optimised to do less sleep/wakeup
  cycles, which should make the drivers not only faster, but more CPU
  friendly too.

Bugfixes
--------

* The basicfuncs module was fixed to work correctly on 32-bit
  architectures.
* The `stored` statistics is no longer incremented by various drivers
  when they mean `processed`.
* The type hinting feature is now more picky about what kind of type
  hints it accepts, allowing one to use template functions in - for
  example - `$(format-json)` pairs.

Miscellaneous changes
---------------------

* We now ship a "Contributors Guide" in the `CONTRIBUTING.md` file.

Credits
-------

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessary to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Andras Mitzki, Balazs Scheidler, Fabien Wernli, Gergely Nagy, Laszlo
Budai, Peter Czanik, Robert Fekete, Tibor Benke, Viktor Juhasz, Viktor
Tusa.

3.6.0alpha2
===========
<!-- Released: Mon, 11 Aug 2014 15:02:59 +0200 -->

This is the second alpha release of the upcoming syslog-ng OSE 3.6
branch, with more internal changes and features compared to the
previous 3.6.0alpha1 release. Above the changes in the latest 3.5.6
stable version, this release contains the following noteworthy
changes:

Changed defaults
----------------

* Threaded mode is now **enabled** by default. To turn it off, use
  `threaded(no)` in the global options section.

* The versioning of the `libsyslog-ng` internal library has changed:
  instead of always using the current release number, we will now try
  to maintain ABI compatibility during the lifetime of a stable
  branch. Therefore, we use only the first two components of our
  version as the base of the library version. Another number will be
  part of the SONAME too, but that will only change when we break
  compatibility.

  The SONAME is currently set to `libsyslog-ng-3.6.so.0`, and will
  remain the same during alpha and beta releases, even when the ABI
  changes. We will start bumping the version after the first stable
  release from this branch, if needed.

Features
--------

* The new `systemd-syslog()` source replaces the former implicit
  support for the same thing. Users who use systemd are advised to use
  either the `system()` source, or this new one when they want to
  receive logs from systemd via the `/run/systemd/journal/syslog`
  socket.

### SMTP destination changes

* The `smtp()` destination now supports a `retries()` option, which
  controls how many times a message delivery will be attempted before
  dropping it.

* The destination no longer counts delivered messages as `stored`.

* The templates used in the destination now honor the time-zone
  settings.

* The driver will abort if required options (any of `to()`, `cc()`,
  `bcc()` and `from()`, and `subject()` and `body()`) are not set.

Bugfixes
--------

* The `file()` and `network()` (including `tcp()` et al) sources will
  now properly set the `$SOURCE` macro.

* To build syslog-ng with the MongoDB destination, libmongo-client
  version 0.1.8+ is now required. (The internal copy has been updated
  accordingly.)

* The UNIX credential extracting feature was ported to FreeBSD,
  syslog-ng now compiles, and has support for this feature. (The
  previous support in alpha1 was sadly incomplete.)

* The `pdbtool merge` command will now generate version 4 patterndb
  files.

Credits
-------

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessary to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Fabien Wernli, Gergely Nagy, Laszlo Budai, Peter Czanik, Tibor Benke,
Viktor Juhasz, Viktor Tusa.

3.6.0alpha1
===========

This is the first alpha release of the upcoming syslog-ng OSE 3.6
branch, a result of about seven months of work, by more than a dozen
contributors, touching 379 files, and changing over twenty thousand
lines.

Compared to the latest stable release (3.5.5), this alpha release
contains the following noteworthy changes:

New dependencies
----------------

PCRE is now a required dependency of syslog-ng, and is not optional
anymore.

Features
--------

### New options

* A new `custom-domain()` global setting was introduced, which allows
  the administrator to override the local domain name used by
  syslog-ng. It affects all locally generated log messages.

* Added a `use-rcptid()` global option, that tells syslog-ng to assign
  a reception ID to each message received and generated by syslog-ng.
  This ID is available as the `$RCPTID` macro, and is unique on a
  given host. The counter wraps around at 48 bits and is never zero.

### New drivers

* The `pseudofile()` destination driver is a very simple driver, aimed
  at delivering messages to special files in `/proc` or `/dev`. It
  opens and closes the file on each message, instead of keeping it
  open. It does not support templates in the filename, and does not
  have a queue (and as such, is not adequate in high traffic
  situations).

* The new `nodejs()` source driver (implemented as an SCL macro) adds
  a source driver that allows syslog-ng to accept messages from
  node.js applications that use the `winston` logging API.

### Miscellaneous new features

* The `multi-line-mode()` option gained a new setting:
  `prefix-suffix`, which works similarly to the `prefix-garbage`
  (which is the new name for `regexp`), except it appends the garbage
  part to the message, instead of discarding it.

  This new mode can be used to work around the absence of a timeout.

* Filters default to PCRE matching, instead of the previous POSIX
  regexp default.

### Statistics

* The stats counter for PROGRAM counters now includes the timestamp of
  the last update.

* A new `stats-lifetime()` global option was introduced, which
  controls how often dynamic counters are expired. The timer is not
  exact, some timers may live a little bit longer than the specified
  time.

* Dynamic counters are now cleaned up every `stats-lifetime()` minutes
  (defaulting to 10 minutes) instead of only on reloads. This change
  was done to reduce the memory used by dynamic counters.

* There is now an `internal_queue_length` statistic, which shows the
  length of the internal queue. This is most useful to see if the
  `internal()` source is not connected, or if it is not being emptied
  fast enough (which, again, indicates a more serious error).

### MongoDB

* The `mongodb()` driver now supports authentication, even when using
  replica sets. When re-connecting to another member of the set, the
  driver will automatically re-authenticate.

* The `--with-libmongo-client` option of the configure script now
  supports `auto` as a value, and will then detect whether to use the
  system version of the library or the internal copy. We default to
  `auto` now, which prefers the system library over the internal copy.

* The driver does not automatically add an `_id` field to the message:
  the server will do that automatically, if none is present. This
  allows users to override the field from within their syslog-ng
  config.

* A new `retries()` option can be used to tell the driver how many
  times it should try to insert a message into the database before
  giving up (defaults to 3). This fixes the case where a rogue message
  could hold up the entire queue, as it was retried forever.

* The driver now enables `safe-mode()` by default.

* There is now a one-minute timeout for MongoDB operations. If an
  operation times out, it will be considered failed.

* The driver can now connect to MongoDB via UNIX domain sockets.

* The `double()` type hint is now supported by the driver.

### Unix Domain Sockets

* The `unix-dgram()` and `unix-stream()` sources now extract UNIX
  credentials (PID, UID and GID of the sending application) from the
  passed messages, if any. On Linux, and FreeBSD, the path of the
  executable belonging to PID is extracted too, along with
  command-line arguments.

  The extracted values are available in `${.unix.pid}`,
  `${.unix.uid}`, `${.unix.gid}`, `${.unix.exe}` and
  `${.unix.cmdline}`, respectively.

* The `system()` source will overwrite the PID macro with the value of
  `${.unix.pid}`, if present.

### JSON

* The json-parser gained an `extract-prefix()` option, which can be
  used to tell the parser to only extract JSON members from a specific
  subtree of the incoming object.

  Example: `json-parser(extract-prefix("foo.bar[5]"));`

  Assuming that the incoming object is named msg, this is equivalent
  to the following javascript code: `msg.foo.bar[5]`

  The resulting expression must be a JSON object, so that syslog-ng
  can extract its members into LogMessage name-value pairs.

  This also works when the top-level object is an array, as
  `extract-prefix()` allows the use of an array index at the first
  indirection level, for example:
  `json-parser(extract-prefix("[5]"));`, which translates to `msg[5]`.

* The `$(format-json)` template function now handles the `double()`
  type hint.

### Debugging

* When sending messages to stderr in debug mode, prepend a timestamp
  to the messages.

* The new `$RUNID` macro is available for templates, which changes its
  value every time syslog-ng is restarted, but not when reloaded.

* A Valgrind suppression file was added (available under
  `contrib/valgrind/`), to aid in debugging memory leaks in syslog-ng.
  It supresses a couple of known false positives, and a few other
  things in third-party libraries.

* A new utility, `system-expand`, was added, which returns what the
  `system()` source would expand to.

Bugfixes
--------

* With the MongoDB destination, successfully inserted messages are not
  counted as "stored" anymore: stored messages are those that are in a
  memory or disk buffer.

* In the MongoDB destination, reconnecting in a replica-set
  environment now works correctly, and reliably.

* The reliability of the `usertty()` destination driver was greatly
  improved. Previously, some parts of it were not thread-safe, which
  could result in strange behaviour.

* The handling of escape related flags of `csvparser()` was changed:
  instead of these flags overwriting all other (even non-escape
  related) flags, if the flag to set is an escape-flag, it will keep
  all non-escape flags, and set the new one. If it is a not such a
  flag, then it will clear all flags, and set the previous escape
  flags, and the new flag.

  This, in essence, means that when setting flags on a `csvparser()`,
  if it is an escape flag, only escape flags will be affected. If not,
  then escape flags will not be affected at all.

* The SQL destination now correctly continues $SEQNUM counting after a
  reload, instead of starting afresh.

* When tring to stop syslog-ng while a reload is in progress,
  syslog-ng will now correctly shut down cleanly.

* When the local hostname is not an FQDN, and the local resolver fails
  to return an FQDN too, syslog-ng does not abort anymore, but
  continues using a non-FQDN hostname after emitting a warning on the
  internal source.

  Furthermore, syslog-ng will try to resolve the FQDN harder: when
  multiple names are returned, it will search for the first FQDN one,
  instead of stopping at the primary name.

* The `update-patterndb` script will now work correctly when the
  current working directory contains .pdb files.

* We will now correctly handle time going backwards in patterndb: it
  will realign its idea of current time with the system. This corrects
  a bug where timeouts did not function properly when system time was
  set backwards.

* The Linux capability support is now correctly auto-detected by the
  configure script, and defaults to off on FreeBSD 9+, as it should.

* Various memory leak fixes around the code base.

Developer notes
---------------

The code base went through a lot of refactoring, too many to list in a
simple NEWS file. Groundwork has been laid out for future features
which are yet to hit the 3.6 branch.

Credits
-------

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessary to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Andras Mitzki, Andres Tamayo, Balazs Scheidler, Csaba Karsai, Daniel
Gados, Evan Rempel, Fabien Wernli, Gergely Nagy, Igor Ippolitov, Imre
Lazar, Jakub Wilk, Laszlo Budai, Lucas McLane, Martin Bagge, Matyas
Koszik, Nick Alcock, Otto Berger, Peter Czanik, Peter Gyongyosi,
Sebastien Badia, Sebastiaan Hoogeveen, Tamas Pal, Tibor Benke, Tobias
Schwab, Viktor Juhasz, Viktor Tusa, Xufeng Zhang
