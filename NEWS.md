3.23.1
======


## Features

 * The `redis()` destination now handles any number of command parameters.
   (#2816)

 * The `format()` option of `date-parser()` supports lists. From now on, a
   single `date-parser()` instance is able to process different date formats,
   making it easy to catch on when some programs change the way they log
   information. (#2779)

 * Add relocation support for `disk-buffer()`. The `relocate` subcommand of
   `dqtool` can be used to move a single or multiple queue files. (#2855)

 * `file(), pipe()`: The `time-reap()` option now can be set or disabled for
   each destination separately.
   Use `time-reap(0)` to disable closing idle destination files. (#2798)

 * `syslog-ng-ctl`: Add `list-files` subcommand to print files present in the
   current configuration. (#2797)

## Bugfixes

 * Fix minor memory leaks (#2868)
 * Add global context to standalone parsers (#2876)
 * Fix heap usage after free in cfg_run_parser_with_main_context (#2884)
 * Fix g_thread_init call order issue with older glibs (#2853)
 * SNMP destination: fixing statistics format (#2854)
 * eventlog: flush escaped_buffer when full (#2837)
 * dbparser: remove unnecessary lock (#2838)
 * dbparser: fix crash when context times out in the middle of another rule (#2832)
 * radix: fix grouping in PCRE (#2808)
 * add-contextual-data: make filters config plugin aware (#2886)
 * Undefined warning regression (#2829)

## Other changes

 * Disable `time-reap()` on non-templated filenames by default (#2798)
 * The `--preprocess-into` command line flag accepts `-`, and writes the
   preprocessed configuration to stdout. (#2869)
 * Add information on environment variables passed to the confgen script (#2888)
 * Light: minor fixes (#2867, #2844)
 * python: use malloc_debug for python unit tests (#2866)
 * Travis: verbose unit test output (#2851)
 * Travis cleanup (#2809)
 * filter: add unit tests to `filter-op` (#2835)
 * Fix clang/gcc diagnostic differences (#2810)
 * Fix leak in stats test (#2874)
 * cmake, autotools: -Wundef to enable-extra-warning  (#2806)

## Notes to the developers

 * Check whether commit messages are properly formatted (#2803, #2807)
 * gitignore: tags file and build directory (#2794)

## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Andras Mitzki, Antal Nemes, Attila Szakacs, Balazs Scheidler, Gabor Nagy,
Laszlo Budai, Laszlo Szemere, László Várady, Mark Bonsack, Mehul Prajapati,
Péter Kókai, Romain Tartière, Zoltan Pallagi.
