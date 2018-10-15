3.18.1
======

## Features

 * HTTP batching (#2135)
 * HTTP multi threading (#2291)
 * HTTP framing support for batches (#2190)
 * Python source (#2308)
 * Template support for Python destination (#2196)
 * HDFS time-reap (#2257)
 * Riemann batching (#2098)
 * flush-timeout() for HTTP (#2251)
 * Base64 encoding template function (#2223)
 * url-decode() template function (#2278)
 * Allow IPv4 mapped addresses in IPv6 mode (#2312)
 * app-parser: set ${.app.name} to the application identified (#2290)
 * Value pairs improvements (#2287)
 * syslog-ng-ctl config to print pre-processed configuration (#2280)
 * Add support for whitespace in template functions (#2271)
 * verify the configuration is the same as on the disk (#2345)

## Bugfixes

 * Fix queue counters race condition (#2316)
 * AMQP configurable max connection and frame size (#2343)
 * Fix wakeup in threaded sources (#2339)
 * Fix libnet memory leak in network destinations (#2331)
 * Fix unexpected flag check-hostname in syslog-parser (#2314)
 * Fix memory leak in dbparser (#2311)
 * Inline Python code comment generates syntax error (#2319)
 * Password protected SSL keys portability (MADV_DONTDUMP) (#2341)
 * Fixing compiler warnings from armv7l (#2301)
 * Riemann crashes in flush (#2296)
 * Revert sticky hook option (#2295)
 * Small stats fixes (#2294)
 * Detect filter loop (#2283, #2288)
 * Fix infinite loop in threaded destinations with ivykis prior 0.39 (#2275)
 * Fix log expr node use after free (#2269)
 * Remove gradle from the list of "BuildRequires" (RPM packaging) (#2266)
 * Fix wildcard-source memleak when directory removed (#2261, #2267)
 * Missing macros: C_AMPM, C_USEC, C_MSEC, C_HOUR12 (#2259)
 * Fix cisco timestamp parsing (#2272)
 * Undefined filter reference (#2273)
 * Fix the literal() type hint (#2286)
 * logwriter, affile, afsocket: fixing "internal overflow". (#2250)
 * lib/gsockaddr.c: modify the unix salen calculation (#2285)
 * dbld: fix dbld/rules deb failure (#2282)
 * Update data type to avoid conversion. (#2281)
 * compat/getent: add support for platforms that lack the r versions (#2244)
 * Fix memory leak caused by saving stats counter to persist config (#2279)
 * Hdfs: disable archive when append-enabled is configured (#2235)
 * scl: add linux-audit() SCL to make files (#2230)
 * DebianPackaging: Add linux-audit SCL to included dirs (#2254)
 * cap_syslog capability detection (#2227)
 * LogProto partial write (#2194)

## Other changes

 * telegram, urlencode: api changes (#2299)
 * python: include python2/3 in plugin description (#2337)
 * Stats prepare for multiple queues per destdrv (#2302)
 * Update deprecated use of tcp()/udp() to network(). This addresses #2322 (#2326)
 * Message about not supported cap_syslog only at debug level (#2333)
 * Few test leak fix (#2323)
 * warning elimination: pointer arithmetics (#2305)
 * templates: get rid off the args_lock (#2289)
 * Ack tracker small refact (#2277)
 * ElasticSearchDestination: Display deprecated warning message about us… (#2274)
 * Improve Readme.md header structure (#2258)
 * Rewrite json tests based on criterion (#2255)
 * Rewrite dbparser tests based on criterion (#2252)
 * Processed timestamp (#2243)
 * msg parameters: remove last NULL parameter from msg macros (#2242)
 * Fix threaded destination test cases (#2236)
 * dbld: missing packages, deps changes (#2232, #2332, #2327, #2260, #2256)
 * Use msg_trace() when emitting trace information (#2226)

## Notes to the developers

 * Threaded source and fetcher (#2247)
 * "Examples" module (#2248)
 * Do not ship mongo-c-driver with syslog-ng (remove as submodule) (#2245)

## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Andras Mitzki, Antal Nemes, Balazs Scheidler, Gabor Nagy, Gergely Tonté,
Laszlo Budai, Laszlo Szemere, László Várady, Maurice T. Meyer, Mahmoud Salama,
Norbert Takacs, Peter Czanik, Peter Gyorko, Peter Kokai, Robert Fekete,
Terez Nemes, Tibor Bodnar, Zoltan Pallagi, y-l-i.
