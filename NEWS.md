3.16.1

<!-- Mon, 18 Jun 2018 14:16:44 +0200 -->

## Features

 * Telegram destination and $(urlencode) template function (#2085)
 * Error reporting on misspelled block args (#1952)
 * New ignore_tns_config Oracle SQL destination config option (#2054)
 * Per-source "src.host" and "src.sender" counters (#2013)

## Bugfixes

 * Fix possible loss of log messages in the systemd-journal() source (#1570, #1587, #1612)
 * Fix file source location information in internal logs (#2028)
 * Fix SDATA deserialization (disk-buffer crash) (#1919, #2067)
 * Fix unaccepted embedded 'file' keyword (file source and destination) (#2076)
 * Fix memory leaks in appmodel and varargs (#2086)
 * Fix a bug in the old LogMessage deserialization (#2103)
 * Fix reading the output of the confgen program (#1780, #2108)
 * Add safer mem_zero() to secret-storage (#2049)
 * Fix undefined behavior in syslog-ng-ctl query (#2043)
 * Fix lloc tracking for multi line blockrefs (#2035)
 * Added missing 'else {};' to default-network-drivers() to forward unparsable messages (#2027)
 * Fix mixed linking (#2020, #2022)
 * Fix compilation of evtlog on FreeBSD (#2014, #2015)
 * Fix thread_id allocation for more than 32 CPUs (crash) (#2008)
 * Add safe logging of errno (#1990, #1999)
 * Fix warnings related to floating point operations (#1959, #1996)
 * Partial revert of plugin discovery to bring back valgrind (#1953, #1995)
 * Fix connection close in network sources (#1991)
 * Fix file deletion in the wildcard-file() source (#1974)
 * Disable the DNS cache if use-dns(no) is used (#1923)
 * Fix compiler error for gcc 4.4 (#2044)
 * Fix emitted warnings due to -no-pie detection for gcc 4.4 (#2037)
 * Fix date format in functional tests (#2036)
 * Dbld fixes (#2034)
 * Rename PAGESIZE variables to pagesize in secret-storage (compilation fix) (#2111)
 * Fix the lifetime of TLSContext to prevent crash on reload (#2080, #2109)
 * Fix reaping program() source and destination when a Java-based destination is used (#2099)

## Other changes

 * Add debug message to program source/destination about successful start (#2046)
 * Report memory exhaustion errors during config parsing (#2033)
 * Improved debug logs (#2032)
 * Dbld coverage (#2031)
 * LogTransportMock enhancement (#2017)
 * Modify the license of loggen from GPL to LGPL (#2006)
 * Loggen refactor (#1987)
 * Update RPM generation (#1980, #2113)
 * Support ENABLE_EXTRA_WARNINGS with CMake (#2072)
 * Rewrite unit tests based on Criterion (#2026, #2058, #2039, #2018, #2003)
 * Lexer test coverage improvements (#2062)
 * preparation for 3.16 OSE rhel/packaging (#2113)

## Notes to the developers

 * Do not ship rabbitmq-c with syslog-ng (remove as submodule) (#2052)
 * Multitransport (#2057)
 * Timeout support for LogReader and LogWriter (#2056)
 * Update ivykis to 0.42.3 (submodule) (#2012)
 * Drop explicit version numbers from requirements.txt (#2050)
 * CMake modernization pt. 1 (#2007)
 * Assert when log_pipe_clone() is not implemented (#2019)
 * Small Java code refactor (#2066)

## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:
Andras Mitzki, Andrej Valek, Antal Nemes, Balazs Scheidler, Gabor Nagy,
Gergely Nagy, German Service Network, Jakub Wilk, Laszlo Budai, Laszlo Szemere,
Laszlo Varady, Mehul Prajapati, Norbert Takacs, Peter Czanik, Peter Kokai,
Tomasz Kazimierczak, Viktor Juhasz
