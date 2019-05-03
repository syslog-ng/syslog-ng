3.21.1
======

## Highlights

 * Add an alternative, native, librdkafka based kafka-c() destination in
   parallel of the existing Java implementation, that provides the same
   configuration interface.  Eventually, we expect this to replace the Java
   one (#2496)

 * Add a native, `http()` based destination based driver for elasticsearch
   called `elasticsearch-http()`, as an alternative of the Java one.
   Eventually, we expect this to replace the Java implementation.  (#2509)

 * Add the ability to automatically determine the timezone value for an
   incoming log entry as long as the incoming stream is close to real time
   and the timezone information is missing from the timestamp.  Enable this
   function by using `flags(guess-timezone)` for sources and the
   date-parser().  (#2517, #2673)

## Features

 * `syslog()`: Add the ability to work with messages larger than `log-msg-size()`
   in the source driver by using the `trim-large-messages(yes)` option.
   The characters over the limit will be truncated.  Previously messages
   longer than the limit caused the connection to be closed abruptly.
   (#2644)

 * `amqp()`: add support for heartbeats and the "external" authentication
   mechanism. (#2676, #2626)

 * `graylog2()`: add support for TLS and UDP. (#2657)

 * `udp()`: Add `spoof-source-max-msglen()` option to allow setting the
   maximum spoofed datagram size, which was hard-wired to 1024 previously.
   (#2535)

 * `db-parser()`: add an option `program-template()` that customizes the
   value used for matching the PROGRAM field. (#2651)

 * `pdbtool`: Add sort option to pdbtool merge (#2664)

 * `$(implode)` and `$(explode)`: add template functions to split and join
   strings based on a simple separator. The exploded array is represented as
   a syslog-ng list that can be manipulated with the $(list-*) template
   functions. (#2700)

 * Add an `--omit-empty-values` option for value-pairs based destinations &
   template functions. (#2519)

 * `grouping-by()` parser: add sort-key() option (#2701)

## Support for non-syslog or non-standard formats in SCL

 * `apache-accesslog-parser()`: support for vhost:port as the first field in
   common/combined log formats (#2688)
 * Add application adapter for Junos classification (#2684)
 * Add parser and adapter for CheckPoint LogExporter output (#2665)

## Bugfixes

 * Fix race condition of idle timer and scheduled I/O job (#2650)
 * Few leaks find via sanitizer (#2696)
 * syslogformat: set $MSG even if the incoming message is empty (#2672)
 * Fix double-free error in logproto unit tests (#2662)
 * groupingby: identical persist name (#2659)
 * stats: deindex pruned counters/clusters (#2648)
 * Type hinting should not accept empty values (#2639)
 * app-parser, pseudofile: fix crash with grammar error (#2640)
 * python: set_timestamp normalization (#2643)
 * db-parser: fix memory leak (#2652)
 * grouping-by: use after free, memory leak, missing init calls of filters (#2655)
 * amqp: fixing double connect (#2660)
 * old style definition warning fixes (#2680)
 * Fix "!=" filter (#2683)
 * dbparser: fix memleak (#2706)
 * nondumpable-allocator: fixing mmap error handling (#2666)
 * Fix timeutils warning (#2604)
 * Fix old style include statement compatibility (#2600)
 * Fix config revert (threaded destinations) (#2596)
 * Add warning on old style include statement (#2592)

## Other changes

 * cfg-parser: add aliases for yesno (#2671)
 * Include json-c in the dist tarball (#2590)
 * cmake: disable_all_modules support (#2647)
 * Cmake clang sanitizer (#2562)
 * timeutils refactor (#2483)
 * Expedite threaded flush at reload (#2656)
 * elasticsearch2: Added deprecation warning (#2628)
 * Astyle fixes (#2624)
 * Force C99 with GNU (#2623)
 * Make rewording and other small edits to README (#2608)
 * Port tests to Criterion (#2607, #2661, #2621, #2620, #2619, #2618, #2617,
   #2616, #2615, #2599, #2594, #2593, #2591, #2586, #2584, #2583)
 * test_reliable_backlog: fix random failure (#2668)
 * Fix unit test with function pointer dereference in case of ASLR, Criterion (#2669)
 * test-stats-query: fix unit test (#2603)

## Notes to the developers

 * Version from git describe (#2627)
 * light: example-msg-generator support (#2571)
 * light: test app parser applications (#2686)
 * light: Switch to native logger (#2546)
 * light: Remove SetupTestcase() dependency  (#2587)

## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:
Andras Mitzki, Antal Nemes, Attila Szakacs, Balazs Scheidler, Chris Spencer,
David Liew, Fabien Wernli, Gabor Nagy, Laszlo Budai, Laszlo Szemere, Layne,
László Várady, Mehul Prajapati, Nik Ambrosch, Parth Wazurkar, Péter Kókai,
Terez Nemes, Victor Ma, Zoltan Pallagi.
