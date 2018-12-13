3.19.1
======

## Features

 * HTTP load balancer (#2347)
 * Slack destination (#2451)
 * Add Cisco Catalyst formatted triplets support to cisco-parser() (#2394)
 * Add RFC5424 syslog support to the system() source (FreeBSD 12.0 support) (#2430)
 * Add network `interface()` option to network sources (#2389)
 * Add so-reuseport() to network drivers (#2379)
 * Enable supporting HTTP redirects (#2136)

## Bugfixes

 * Fix frequent disconnects of syslog() with TLS (#2432)
 * Fix possible refcount leak during reload/shutdown (#2434)
 * Fix message storm on trace level (#2425)
 * Fix use after free in file destinations (time-reap) (#2418)
 * Fixing a few memleaks in the Java destination (#2417)
 * Fix telegram dst default ca dir (#2416)
 * Fix prefix handling in $(list-concat) and $(strip) (#2405)
 * Fixing an eventfd leak with ivykis<=0.38 (threaded destinations) (#2404)
 * Process flush result after worker thread exits (threaded destinations) (#2402)
 * hdfs: do not try to write unopened file (#2391)
 * Fix leaks in redis() destination (#2383)
 * Block location tracking fixes (#2378)
 * Fix $(basename) and $(dirname) in the presence of a prefix (#2371)
 * Fixing a false positive corruption detection in non-reliable diskq (#2356)
 * Check if /proc/kmsg can be opened in system-source (#2408)
 * Fix include guard in systemd-journal (#2445)
 * Remove hexadecimal and octal number parsing from templates (#2401)

## Other changes

 * Do not load certs from default CURL ca-dir by default (http() destination) (#2410)
 * Disable SSL compression by default (#2372)
 * Flush lines cleanup (#2386, #2392)
 * Refine json-parser() log messages to be less alarming (#2437)
 * Move some messages to trace (#2358)
 * Make include-path more discoverable (#2426)
 * Adding build flag -Wmissing-format-attribute and -Wsuggest-attribute=noreturn (#2423)
 * Rewrite filter unit tests based on criterion (#2422)
 * PytestFramework in Travis (#2415)
 * syslog-ng-mod-java debian pkg should depend on headless jre (#2388)
 * Add contextual data error reporting improvements & csv-scanner refactor (#2373)
 * Afsocket remove unused functions/bitfields (#2363)
 * Afsocket minor cleanup/refactor (#2355)
 * Riemann worker (#2313)
 * Afsql threaded dest driver (#2097)
 * dbld: do not mount .gitconfig if missing (#2419)
 * dbld: Add missing docbook-xsl packages (#2398)
 * dbld: update criterion to 2.3.3 (#2396)
 * dbld: Remove "proposed" Ubuntu repository from enable_dbgsyms() (#2382)
 * dbld: Add new target "list-builder-images" (#2381)
 * dbld: Support Ubuntu Bionic and update existing images (#2318)
 * dbld: release target should use the default image (#2464)

## Notes to the developers

 * PytestFramework: Add initial test framework (#1940)

## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Abder Benbachir, Andras Mitzki, Antal Nemes, Attila Szakacs, Balazs Scheidler,
Gabor Nagy, Gergely Tonté, JP Vossen, Juhasz Viktor, Laszlo Budai,
Laszlo Szemere, László Várady, Norbert Takacs, Peter Kokai, Zoltan Pallagi.
