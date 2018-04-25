3.15.1

<!-- Thu, 19 Apr 2018 10:55:16 +0200 -->

## Features

 * Support added for `if`/`elif`/`else` blocks to the configuration file syntax.
   (#1856)
 * Dramatically improved debug messages during filter/parser evaluation. (#1898)
 * Similarly improved the error messages shown on syntax errors, they now show a
   full backtrace of inclusions, among other things. (#1932)
 * The `hook-commands` module was added, allowing one to run custom commands on
   source or destination setup and teardown. (#1951)
 * Implemented a way to skip processing included config file snippets in case a
   dependency is missing: The `@requires json` pragma. (#827, #1956)
 * Basic client-side failover support was implemented. (#1905)
 * Errors from python destinations are now reported together with any exception
   text (if any). (#1931)
 * `add-contextual-data` gained a new `ignore-case()` option. (#1911)

## Bugfixes

 * Fix a crash that happened on disk queue restart. (#1886)
 * Fixed another crash when a corrupted disk queue file was being moved away.
   (#1924)
 * Fixed a crash that could happen during nvtable deserialization. (#1967)
 * Fixed a crash that occurred when NVTables were stored on low memory
   addresses. (#1970)
 * Fixed an issue with TLS session resumption, the session id context value is
   now properly set. (#1936, #2000)
 * We now link directly to the `evtlog` shipped with syslog-ng, and are not
   using the system library, not even when present. (#1915)
 * TLS destinations now work again without `key-file` or `cert-file` specified.
   (#1916, #1917)
 * SDATA block names are now sanitized, in order to not break the spec when we
   get our SDATA from sources that are more lax (such as JSON). (#1948)
 * Some internal messages contained key-value pairs where the key had spaces in
   it, this has been addressed, they do not contain spaces anymore.
 * The STOMP destination will now correctly use template options when formatting
   its body part. (#1957)
 * Fix compilation with OpenSSL 1.1.0 (#1921, #1997)
 * Fix compilation on FreeBSD. (#1901)
 * Fix compilation on SLES 11. (#1897)
 * Fix compilation on Hurd. (#1912, #1914)
 * Fix compiltaion on Solaris 10. (#1982, #1983)
 * Fix compilation on MacOS.
 * Fixed a value conflict in the `afstreams` module's grammar file.
 * Various compiler warning-related fixes all over the codebase.

## Other changes

 * POSIX RegExp support was dropped from the filters, PCRE remains available. (#1899)
 * Miscellaneous build-system related fixes and improvements (both autotools and
   CMake).
 * Update `lib/json-c` to `json-c-0.13-20171207`. (#1900)

## Notes to the developers

 * The `init()` function is now optional for Python destinations. (#1756)
 * The Docker environment (`dbld/`) has seen significant changes, among them an
   upgrade to Ubuntu Xenial. (#1876)
 * `dbld/rules` gained two new targets: `login` and `build`, that do what their
   names suggest. (#1927)
 * The `LogPipe` object gained a `pre_init()` and a `post_deinit()` method, used
   by the `hook-commands` module.

## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:
Andras Mitzki, Antal Nemes, Balazs Scheidler, Budai Laszlo, Gabor Nagy, Gábor
Nagy, Gergely Nagy, Juhasz Viktor, Kókai Péter, Laszlo Budai, László Szemere,
László Várady, Mehul Prajapati, Norbert Takacs, Robert Fekete, SZALAY Attila,
Tamas Nagy, Terez Nemes, Utsav Krishnan, Videet Singhai, Vivek Raj
