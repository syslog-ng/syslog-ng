3.22.1
======

## Highlights

 * Sending SNMP traps: Using the new `snmp()` destination, incoming log messages
   can be converted to SNMP traps, as the fields of the SNMP messages can be
   customized with macros. (#2693)

 * `$(template)` dynamic binding: Extends the $(template) template function to
   allow dynamic binding. For example, the name of the template to be invoked
   can come from the message (name-value pairs). (#2716)

 * `syslog()`, `network()`: Add `dynamic-window-size()` option to enable dynamic
   flow control that distributes the specified amount of window between active
   connections at runtime. This can be used in low-memory environments, where
   only a small subset of the active clients sends messages at high rate.
   (#2772)

## Features

 * `match()`: Add support for the `template()` option (#2715)
 * `add-contextual-data()`: Allow using templates in name-value pairs (#2711)
 * Add support for floating point operations in template functions (#2742)
 * Add support for usec precision when parsing time (#2709)

## Bugfixes

 * Fix null pointer access when destinations are suspended (#2778)
 * Fix `grouping-by()` deadlock (#2758)
 * Fix a general source-related crash and enhance `wildcard-file()`'s bookmark
   handling (#2589)
 * Fix infinite loop (reload/reopen) (#2739)
 * Fix `python()` package/module name collision (#2438)
 * Fix escaped quote in block argument (#2781)
 * Reintroduce test on SYSLOG_NG_HAVE_TIMEZONE (#2774)
 * `snmp()`: Fix template leak (#2746)

## Other changes

 * Never drop flow-controlled messages: The meaning of `log-fifo-size()` has
   changed to avoid dropping flow-controlled messages when `log-fifo-size()` is
   misconfigured. From now on, `log-fifo-size()` only affects messages that are
   not flow-controlled. (#2753)

 * The `-d`/`--debug` syslog-ng command line flag no longer implies
   `-e`/`--stderr`. If you want to redirect `internal()` source to stderr,
   use the `-e`/`--stderr` option explicitly. (#2731)

 * dbld, RPM and DEB packaging improvements (#2724)
 * Checkpoint parser improvements (#2740)
 * Reset the timezone on config reload event (#2691)
 * `geoip2()`: Include IP into the error message (#2743)
 * Improve regexp error messages (#2796)
 * `http()`: Warn if less workers used than urls (#2757)
 * `http()`: Allow URLs to be specified by a space/comma separated string
   (#2699)
 * loggen: Change message rate at runtime using signals (#2756)
 * debun: add acquire_running_syslog_config function (#2752)
 * FreeBSD fixes for the test suite (#2783)

## Notes to the developers

 * ivykis: update to 0.42.4 (#2736)
 * Support generator plugins in global options (#2747)
 * logthrfetcher: new constants (#2766)
 * logthrsourcedrv: support position tracking (#2750)
 * Light: Support pre-commit and tox (#2725)
 * Enable Bison error flags: conflicts-sr/rr (#2762)
 * Dynamic stats constant registration (#2761)

## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:
Andras Mitzki, Antal Nemes, Attila Szakacs, Balazs Scheidler,
Christian Michallek, Fabien Wernli, Gabor Nagy, Kyeong Yoo, Laszlo Budai,
Laszlo Szemere, László Várady, Mehul Prajapati, Norbert Takacs, Oleksii Hamov,
Péter Kókai, Romain Tartière, Zoltan Pallagi.
