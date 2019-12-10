3.25.1
======

## Highlights

 * `http-destination`: Users now can specify the action for any HTTP result code.
   Use with `response-action(response_code => action)` in your http block.
   Available actions are: `success`, `retry`, `drop` and `disconnect`. (#3007)
 * `syslog-ng-cfg-db`: Added a new script, which can provide the options of
   sources and destinations queried by the user. This tool can make the configuration
   of syslog-ng a lot easier. Use with `./syslog-ng-cfg-db.py` from the
   `contrib/config_database` dir.(#2997)
 * `redis-destination`: Improved the performance by 2 orders of magnitude.
   In our labor environment, now it operates at 25k EPS. (#2972)

## Features

 * `create-dirs()`: Added to `pipe()` source/destination, and standardize the behavior.
   (#3018, #2635)
 * `default-network-drivers`: Added `max-connections()` option, to change the limit
    from 10. (#2961)
 * `checkpoint`: Added support for timezone value at the end of timestamps. (#3033)
 * `filter/rewrite`: Added `disable-jit` flag to disable JIT PCRE compilation. (#2992, #2986)
 * `syslog-ng-ctl`: Added `export-config-graph` option to visualize config graph. (#2990)
 * `build/travis`: Added ARM64 arch support. (#2967) 
 * `build/dbld`: Readded CentOS 6 support. (#2860, #2971, #3028)
 * `python`: Added Python 3.8 support. (#3017)

## Bugfixes

 * `tls`: Fixed an infinite loop which occured, when a `TLS` connection broke. (#3026, #3009)
 * `log-block`: Fixed an issue, where inline `network` destinations disjointed
   the rest of the config. (#2989, #2820)
 * `kafka/network-load-balancer`: Fixed a crash when an argument was set to empty. (#3002)
 * `python-source`: Fixed a memory corruption during reload. (#3014)
 * `python-destination`: Actually use return value of `open` method. (#2998, #2513)
 * `python-fetcher`: Fixed `FETCH_NO_DATA` and `FETCH_TRY_AGAIN` constants. (#3012)
 * `python`: Fixed python `Exception` reporting when no `Exception` happened. (#2995)
 * `telegram`: Fixed the syntax error of the `use-system-cert-store()` option. (#2977)
 * `config`: Throw error to single dots, which were ignored before. (#3000)
 * `file-destination`: Delay ACKs until messages are written to disk. This fixes message
   drop on I/O error and message lost in the LogProtoFileWriter in case of a crash, by
   retrying to send the message. (#2985)
 * `http-destination`: Handle global template options values. (#3020)
 * `timeutils`: Fixed month and day name parsing, when only the first 2 characters
    matched. (#3035)
 * `logmsg`: Added default `PRI` value (`LOG_USER | LOG_NOTICE`) to log messages
   created without initial parsing. (#2974)
 * `packaging`: Added ordering dependencies `network.target` and `network-online.target`
   to the service files. (#2994, #2667)
 * `amqp`: Support older (0.7.1) version (#2999)
 * `loggen`: Set plugin path in installation time. (#3019)
 * `timeutils/patterndb`: Fixed some undefined behaviours. (#2969)
 * `stomp`: Fixed a buffer over-read on connection. (#2988)
 * `pseudofile`: Fixed a crash, when `template()` option is not set. (#2988)
 * `wildcard-source`: Fixed a crash, when `max-files()` was set to 0. (#2988)

## Other changes

 * `syslog-ng-debun`: Various maintenance updates and small fixes. (#2993)
 * `scl`: Avoid `@requires` loading the plugins themselves. (#2887)

## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Andras Mitzki, Antal Nemes, Attila Szakacs, Balazs Scheidler, Clément Besnier,
Gabor Nagy, jadhavsumit98, Janos Szigetvari, Laszlo Budai, Laszlo Szemere,
László Várady, MikeLim, Nikita Uvarov, Norbert Takacs, pabloli, Péter Kókai,
Zoltan Pallagi.
