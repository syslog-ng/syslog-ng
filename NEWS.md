3.29.1
======

## Highlights

 * `panos-parser()`: parse Palo Alto PAN-OS logs

   Example:
   ```
   @include "scl.conf"

   log {
     source { network(transport("udp")); };

     parser { panos-parser(); };

     destination {
      elasticsearch-http(
        index("syslog-ng-${YEAR}-${MONTH}-${DAY}")
        type("")
        url("http://localhost:9200/_bulk")
        template("$(format-json
          --scope rfc5424
          --scope dot-nv-pairs --rekey .* --shift 1 --exclude *future_* --exclude *dg_hier_level_*
          --scope nv-pairs --exclude DATE --key ISODATE @timestamp=${ISODATE})")
      );
     };
   };
   ```
   ([#3234](https://github.com/syslog-ng/syslog-ng/pull/3234))

## Features

 * snmptrap: improve error message when missing dependency
   ([#3363](https://github.com/syslog-ng/syslog-ng/pull/3363))
 * disk queue: reduce memory usage during load
   ([#3352](https://github.com/syslog-ng/syslog-ng/pull/3352))
 * config: support `@version: current`
   ([#3368](https://github.com/syslog-ng/syslog-ng/pull/3368))
 * Allow dupnames flag to be used in PCRE expressions, allowing duplicate names for named subpatterns
   as explained here: https://www.pcre.org/original/doc/html/pcrepattern.html#SEC16 .

   Example:
   ```
   filter f_filter1 {
     match("(?<FOOBAR>bar)|(?<FOOBAR>foo)" value(MSG) flags(store-matches, dupnames));
   };
   ```
   ([#3381](https://github.com/syslog-ng/syslog-ng/pull/3381))

## Bugfixes

 * filter/regex: if there was a named match (?<named>..)? that is optional to match, the previose or the next named matches might not be saved as named match.
   ([#3393](https://github.com/syslog-ng/syslog-ng/pull/3393))
 * `tls`: Fixed a bug, where `ecdh-curve-list()` were not applied at client side.
   ([#3356](https://github.com/syslog-ng/syslog-ng/pull/3356))
 * scratch-buffers: fix `global.scratch_buffers_bytes.queued` counter bug
   This bug only affected the stats_counter value, not the actual memory usage (i.e. memory usage was fine before)
   ([#3355](https://github.com/syslog-ng/syslog-ng/pull/3355))
 * wsl: fix infinite loop during startup
   ([#3340](https://github.com/syslog-ng/syslog-ng/pull/3340))
 * `openbsd`: showing grammar debug info for openbsd too, when `-y` command line option is used
   ([#3339](https://github.com/syslog-ng/syslog-ng/pull/3339))
 * `stats-query`: speedup `syslog-ng-ctl query get "*"` command.

   An algorithmic error view made `syslog-ng-ctl query get "*"` very slow with large number of counters.
   ([#3376](https://github.com/syslog-ng/syslog-ng/pull/3376))
 * syslogformat: fixing crashing with small invalid formatted logs see example in #3328
   ([#3364](https://github.com/syslog-ng/syslog-ng/pull/3364))
 * `cfg`: fix config reload crash via introducing `on_config_inited` in LogPipe
   ([#3176](https://github.com/syslog-ng/syslog-ng/pull/3176))
 * config: fix error reporting

   - Error reporting was fixed for lines longer than 1024 characters.
   - The location of the error was incorrectly reported in some cases.
   ([#3383](https://github.com/syslog-ng/syslog-ng/pull/3383))
 * `disk queue`: fix possible crash during load, and possible false positive corruption detection
   ([#3342](https://github.com/syslog-ng/syslog-ng/pull/3342))
 * db-parser, pdbtool, graphite-output: fix glib assertion error

   The assertion happened in these cases
   * dbparser database load
   * argument parsing in graphite-output
   * pdbtool merge commad

   Syslog-ng emitted a glib assertion warning in the cases above, even in successful executions.

   If `G_DEBUG=fatal-warnings` environment variable was used, the warning turned into a crash.
   ([#3344](https://github.com/syslog-ng/syslog-ng/pull/3344))
 * stats: fix stats-ctl query crash when trying to reset all the counters
   `syslog-ng-ctl query get '*' --reset`
   ([#3361](https://github.com/syslog-ng/syslog-ng/pull/3361))

## Packaging

 * RHEL 7 packaging: fix logrotate file conflict with rsyslog
   ([#3324](https://github.com/syslog-ng/syslog-ng/pull/3324))
 * Debian packaging: python3-nose was removed from package dependencies.
   Pytest will run Python related unittests (for modules/python/pylib/syslogng/debuggercli/tests/)
   instead of nose.
   ([#3343](https://github.com/syslog-ng/syslog-ng/pull/3343))

## Notes to developers

 * light: test for assertion errors in glib for each testcases
   ([#3344](https://github.com/syslog-ng/syslog-ng/pull/3344))
 * Fix signal handling when an external library/plugin sets SIG_IGN

   Previously, setting SIG_IGN in a plugin/library (for example, in a Python module) resulted in a crash.
   ([#3338](https://github.com/syslog-ng/syslog-ng/pull/3338))
 * `func-test`: removed logstore_reader check, which was never reached
   ([#3236](https://github.com/syslog-ng/syslog-ng/pull/3236))
 * `plugin_skeleton_creator`: fixing a compiler switch

   Wrong compiler switch used in `plugin_skeleton_creator`. This caused a compiler warning. The grammar debug info did not appear for that module, when `-y` command line option was used.
   ([#3339](https://github.com/syslog-ng/syslog-ng/pull/3339))
 * Light test framework: get_stats and get_query functions to DestinationDriver class

   Two new functions added to DestinationDriver class which can be used for getting the stats
   and query output of syslog-ng-ctl.
   ([#3211](https://github.com/syslog-ng/syslog-ng/pull/3211))

## Other changes

 * `internal()`:  limit the size of internal()'s temporary queue

   The `internal()` source uses a temporary queue to buffer messages.
   From now on, the queue has a maximum capacity, the `log-fifo-size()` option
   can be used to change the default limit (10000).

   This change prevents consuming all the available memory in special rare cases.
   ([#3229](https://github.com/syslog-ng/syslog-ng/pull/3229))
 * network plugins: better timer defaults for TCP keepalive

   From now on, syslog-ng uses the following defaults for TCP keepalive:
   - `tcp-keepalive-time()`: 60
   - `tcp-keepalive-intvl()`: 10
   - `tcp-keepalive-probes()`: 6

   Note: `so-keepalive()` is enabled by default.
   ([#3357](https://github.com/syslog-ng/syslog-ng/pull/3357))

## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:
Andras Mitzki, Antal Nemes, Attila Szakacs, Balazs Scheidler, Christian Tramnitz, chunmeng, Gabor Nagy, Laszlo Budai, Laszlo Szemere, László Várady, MileK, Norbert Takacs, Peter Czanik, Péter Kókai, Terez Nemes.
