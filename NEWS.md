3.24.1
======

## Highlights

 * Add a new template function called `$(format-flat-json)`, which generates
   flattened json output. This is useful for destinations, where the json
   parsing does not handle nested json format. (#2890)
 * Add ISO 8601 compliant week numbering. Use it with the `${ISOWEEK}` macro
   and and all its variants: `S_ISOWEEK`, `R_ISOWEEK` and `C_ISOWEEK`. (#2878)
 * Add `add-contextual-data()` glob selector. It matches the message with shell
   style globbing. Enable it by setting `selector(glob("$my_template")` in the
   `add-contextual-data()` block. (#2936)
 * Add new rewrite operations to manipulate the timezone portion of timestamps have
   been added. `set-timezone()` to set the timezone value to a specific value,
   `fix-timezone()` to fix up an incorrectly recognized timezone and `guess-timezone()`
   to automatically deduce the timezone value on the assumption that the message
   is received in near real time. (#2818)
 * Send Server Name Identification (SNI) information with `transport(tls)`.
   Enable it by setting the `sni(yes)` option in the `tls` block in your
   `destination`. (#2930)

## Features

 * `templates`: change the `$LOGHOST` macro to honour `use-fqdn()` (#2894)
 * Define `syslog-ng-sysconfdir` (#2932)
 * `dqtool`: add assign dqfile to persist file feature (#2872)

## Bugfixes

 * Fix backtick subsitution of defines/environment variables in the main configuration file. (#2906, #2909)
 * Fix SCL block parameter substitution of quoted escaped newline (#2901)
 * `python, diskq, random-generator source`: crash after failed reload (#2907)
 * Fix crash at shutdown on 32bit systems (#2893, #2895)
 * Invalidate the value of the `LEGACY_MSGHDR` macro in case either the `PID` or the `PROGRAM`
   macros are `unset()` using a `rewrite` rule. Previously `LEGACY_MSGHDR` would retain the old values. (#2896)
 * on 32bit platform `diskq` ftruncate could fail due to size 32/64 interface (#2892)
 * Support new tzdata format, starting from version 2009.XXX, in tzinfo parser. (#2898)
 * `udp, udp6, tcp, tcp6, syslog, network destination`: Correctly detect and set `IP_MULTICAST_TTL`
   in case of multicast ip address (#2905)
 * Fix hostname resolve on systems with only the loopback network interface configured (#2933)
 * `wildcard-file()`: Add `multi-line()`, `pad_size()` and `multi-line-mode()` option validation. (#2922)
 * `kafka-c`: Fix multiple memleaks (#2944)

## Other changes

 * `geoip`: remove deprecated module, `geoip2` database location detection (#2780)
 * various refactor, build issue fixes (#2902)

## Notes to the developers

 * `LightRunWithStrace`: Run syslog-ng behind strace (#2921)
 * `LightVerboseLogOnError`: Increase default pytest verbosity on error (#2919)
 * Dbld image caching (#2858)
 * Dbld gradle caching (#2857)
 * `logreader,logsource`: move `scratch-buffer` mark and reclaim into `LogSource` (#2903)

## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Andras Mitzki, Antal Nemes, Attila Szakacs, Balazs Scheidler, Bertrand Jacquin,
Gabor Nagy, Henrik Grindal Bakken, Kerin Millar, kjhee43, Laszlo Budai,
Laszlo Szemere, László Várady, Péter Kókai, Raghunath Adhyapak, Zoltan Pallagi.
