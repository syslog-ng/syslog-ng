3.11.1

<!-- Tue, 25 Jul 2017 13:43:52 +0200 -->

# Features

 * Add geoip2 parser and template function.
   It is based on the libmaxminddb(MaxMindDB).
   It will replace the old geoip parser and template function,
   so they are deprecated from 3.11 (but still available).

 * Add SSL support to AMQP.

 * Add template option to apache-accesslog-parser.

 * Add configurable event time to Riemann destination.

 * Add drop-unmatched() option to dbparser.

 * Add Ubuntu Xenial to the bundled docker images.

 * Support multi-instance support for Solaris 10 and 11.

 * Support multi-instance for systemd.

 * Add configurable timeout to HTTP destination.

 * Add prefix() option to cisco-parser.

# Bugfixes

 * Fix a memory usage counter underflow for threaded destination drivers
   and writers.

 * Fix a potential crash in AMQP.

 * Fix a potential crash during reload.

 * Fix a reload/shutdown issue.
   Under heavy load, worker might never exit from the fetch loop from the
   queue.

 * Fix a potential crash in afsocket destination during reload.

 * Fix a counter registration bug.
   In some cases not all the required counters are registered.

 * Fix a build issue on FreeBSD.

 * Fix a memory leak in diskq plugin.

 * Fix systemd-journal error codes validation.

 * Fix a potential crash in diskq when it is used with file
   destination and the file is reaped.

 * Fix a memory leak in HTTP destination

 * Fix ENABLE_DEBUG in dbparser.

 * Fix a unit tests that caused build issue on 32 bit platforms.

# Other changes

 * The eventlog library is part of syslog-ng from now.

 * Improve error messages when the config cannot be initialized.

 * Improve source suspended/resumed debug messages.

 * Rename syslog-debun to syslog-ng-debun.

 * Update manpages to v3.11

 * Remove tgz2build directory.

# Notes to the Developers

 * Rewrite merge-grammar script from Perl to Python.

# Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Andras Mitzki, Antal Nemes, Attila Szalay, Balazs Scheidler, Fabien Wernli,
Gabor Nagy, Giuseppe D'Anna, Janos Szigetvari, Laszlo Budai, Laszlo Varady,
Lorand Muzamel, Mate Farkas, Noemi Vanyi, Peter Czanik, Tamas Nagy,
Tibor Bodnar, Tomasz Kazimierczak, Zoltan Pallagi

