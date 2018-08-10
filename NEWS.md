3.17.2
======

## Bugfixes

 * Fix a bug in flow-control (#2224)
 * Fix template function evaluation in debugger (#2220)

3.17.1
======

## Features

 * Client side failback mode (#2183)
 * New linux-audit() source as SCL (#2186)
 * Decorating generated configuration (#2218)
 * Introduce ewmm() source (#2199, #2209)
 * Add parsing of Cisco unified call manager (#2134)
 * Mandatory parameters for cfg-block (SCL) (#2088)

## Bugfixes

 * dqtool cat print backlog items (#2213)
 * Rewind backlog in case of stateless LogProtoClient (#2214)
 * Filter out incorrectly parsed sudo logs (#2208)
 * Minor fixes related to client-lib-dir, loggen and eventlog (#2204)
 * Minor stats-query fixes and refactor (#2193)
 * Reliable disk buffer non-empty backlog (#2192)
 * Fix pip package versions on older distro releases (dbld) (#2188)
 * Fix a groupset/groupunset and map-value-pairs() crash (#2184)
 * Make g_atomic_counter_set() atomic and update minimum glib version to 2.26 (#2182)
 * Aligning java related SCLs with mandatory parameters (#2160)
 * Loggen minor fixes (#2150)
 * grab-logging should be installed as a header (#2151)
 * Fix possible underflow of memory_usage (afsql, logqueue-fifo) (#2140)
 * Fix SELinux module version inconsistency (#2133)
 * Fix CMake unit test compilation (no-pie) (#2137)
 * Fix possible crash in syslog-parser() (#2128)
 * Disable ack for mark mode (#2129)
 * Fixing a Telegram destination bug with bot id (#2142)
 * All drivers should support inner destination or source plugins (#2143)
 * Fix default file and directory creation ownership (#2119)
 * Fix global "center;;received" counter when systemd-journal() is used (#2121)
 * Link everything to libsecret-storage (#2100)
 * Inform about the right dns-cache() configuration (warning message typo) (#2145)
 * Adjusting window size for internal mark mode (#2146)
 * Fix memory leaks in disk-buffer() (#2153)
 * Fix undefined behavior in log multiplexer (#2154)
 * Fix static linking mode (autotools) (#2155)
 * Fix internal mark mode infinite loop with old ivykis (#2157)
 * Fix missing normalize flags (#2162)
 * Keep JVM running on reload if once configured (#2164, #2211)
 * Fix a race condition (suspend) in LogSource (#2167)
 * Add `@requires json-plugin` to the cim() parser (#2181)
 * Added exclude_kmsg option to system source (#2166)
 * Fix padding template function (#2174)
 * Leak & invalid memory access (#2173)
 * FreeBSD 11.2 builderror SOCK_STREAM (#2170)
 * Add ref-counted TLSVerifier struct (use after free fix) (#2168)

## Other changes

 * Improve loggen's file message parser (#2205)
 * syslog-ng-debun improvements (#2201)
 * Goodbye "goto relex" (refactor) (#2198)
 * Refactor the callback registration mechanism of WildcardFileReader (#2185)
 * Extended Linux capabilities detection (pkg-config) (#2169)
 * Add atomic gssize (#2159)
 * Lower the message level of `@requires` to debug (#2147)
 * macOS warning elimination (#2139)
 * Remove a misleading rewrite-related debug message (#2132)
 * Minor updates to SELinux policy installer script (#2127)
 * More robust GLib detection (CMake) (#2125)
 * Logthreaded nonfunctional changes (#2123)
 * Confgen and pragma improvements (#2122)
 * Flush before stopping syslog-ng (functional tests) (#2216)
 * Port unit tests into criterion (test_filters_netmask6, test_findeom, csv_parser, patternDB) (#2217, #2175, #2118)
 * Libtest refactors (#2149)
 * Add missing files to the source tarball (#2114)
 * Better python binary detection (#2092)

## Notes to the developers

 * LogThreadedDestDriver batching (#2063)
 * Add sqlite3 and riemann to dbld devshell (#2210)
 * Make mock-transport inheritable (#2120)

## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Andras Mitzki, Antal Nemes, Balazs Scheidler, Bernie Harris, Bertrand Jacquin,
Gabor Nagy, Gergely Nagy, German Service Network, Janos SZIGETVARI, Laszlo Budai,
Laszlo Szemere, László Várady, Norbert Takacs, Peter Czanik, Peter Kokai,
Szigetvari Janos, Terez Nemes, Viktor Juhasz.

