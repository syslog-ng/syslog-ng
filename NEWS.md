3.13.1

<!-- Thu, 30 Nov 2017 13:38:22 +0100 -->

# Features

 * Add app-parser() framework (automatic parsing of log messages) (#1689)
 * Support microseconds in Riemann destination (#1710)
 * Add osquery destination as an SCL plugin (#1728)
 * Add network load balancer destination (#1706)
 * Add possibility to only signal re-open of file handles (SIGUSR1) (#1530)
 * It is possible from now to limit the number of registered dynamic counters (#1743)
 * Add $(binary) template function (#1679)
 * Add experimental transport for transferring messages in whole between syslog-ng instances (EWMM) (#1689)
 * Docker based build and debian package generation (#1783)
 * Add auto-parse(yes/no) to app-paser(), system() and default-network-drivers() (#1788)
 * Add Graylog2 destination and $(format-gelf) template function (#1680)

# Bugfixes

 * Exit when a read fails on an included config file instead of
   starting up with an empty configuration. (#1721)
 * Fix double free (#1720)
 * Add missing discarded counter to groupingby (#1748)
 * Fix a reference leak in Python destination (#1716)
 * Fix timezone issue in snmptrapd parser (#1746)
 * Fix potential crash in stdin driver (#1741)
 * Fix a crash when initializing new config fails for socket with keep_alive off (#1723)
 * Fix filter evaluation in case of contexts with multiple elements (#1718)
 * Various grouping-by fixes (#1718)
 * Fix potential use after free around dns-cache during shutdown (#1666)
 * Fix access to indirect values within Java destination (#1732)
 * Fix a crash in affile (#1725)
 * Fix a memory leak (#1724)
 * Fix a crash when getent is used empty group (#1691)
 * Fix jvm-options() (#1704)
 * Fix a crash in Python language binding (#1694)
 * Fix a crash in afmongodb (#1765)
 * Fix a memory leak in afmongodb (#1766)
 * Fix name-to-GID calculation in the $(getent) template function (#1764)
 * Fix a crash when redis is configured without the command() option (#1767)
 * Fix a race condition in kv-parser() (#1789)

# Other changes

 * Cleanup diskq related warning messages (#1752)
 * Provide tls block for tls options in amqp(), http(), riemann() destination drivers (#1715)
 * It it possible from now to register blocks and generators as plugins (#1657)
 * Drop compatiblity with configurations below 3.0 (#1709)
 * Do not change permissions of a file by default (#1782)
 * Allow source files to specify permissions locally (#1782)
 * Minor performance improvement (#1729)
 * The current config version can be queried with "--version" (#1740)
 * Increase the performance of kv-parser() (#1789)

# Notes to the developers

 * Change configure default option for jsonc and mongoc from auto to internal (#1735)
 * Disable ASLR when running unit tests (#1753)

# Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:
Andras Mitzki, Antal Nemes, Attila Szalay, Balazs Scheidler, Gabor Nagy,
Jakub Jankowski, Janos Szigetvari, Laszlo Budai, Laszlo Varady, Laszlo Szemere,
Marton Illes, Mate Farkas, Peter Kokai, Pontus Andersson, Sam Stephenson,
Sebastian Roland, Viktor Juhasz, Zoltan Pallagi.

