3.32.1
======

## Highlights

 * `mongodb()`: add `workers()` support (multi-threaded connection pooling)
   
   The MongoDB driver now supports the `workers()` option, which specifies the
   number of parallel workers to be used.
   Workers are based on the connection pooling feature of the MongoDB C library.
   
   This increases the throughput of the MongoDB destination driver.
   
   Example:
   
   ```
   destination {
     mongodb(
       uri("mongodb://hostA,hostB/syslog?replicaSet=my_rs&wtimeoutMS=10000&socketTimeoutMS=10000&connectTimeoutMS=10000&serverSelectionTimeoutMS=5000")
       collection("messages")
       workers(8)
     );
   };
   ```
   ([#3621](https://github.com/syslog-ng/syslog-ng/pull/3621))
 * `mongodb()`: template support for the `collection()` option
   
   The `collection()` option of the MongoDB destination driver now accepts
   templates, for example:
   
   ```
   destination {
     mongodb(
       uri("mongodb://host/syslog")
       collection("${HOST}_messages")
     );
   };
   ```
   ([#3621](https://github.com/syslog-ng/syslog-ng/pull/3621))

## Features

 * `time-reopen`: Support the `time-reopen()` option on the driver level for the following drivers:
    * sources: `example-diskq-source`, `python-fetcher`
    * destinations: `amqp`, `example-destination`, `file`, `http`, `mongodb`, `network`, `pipe`,
                    `program`, `pseudofile`, `python`, `redis`, `riemann`, `smtp`, `sql`, `stomp`,
                    `syslog`, `tcp`, `tcp6`, `udp`, `udp6`, `unix-dgram`, `unix-stream`, `usertty`
   ([#3585](https://github.com/syslog-ng/syslog-ng/pull/3585))
 * `csv-parser()`: add drop-invalid() option along with the already existing
   flag with the same name. This is to improve the consistency of the
   configuration language.
   ([#3547](https://github.com/syslog-ng/syslog-ng/pull/3547))
 * `usertty() destination`: Support changing the terminal disable timeout with the `time-reopen()` option.
   Default timeout change to 60 from 600. If you wish to use the old 600 timeout, add `time-reopen(600)`
   to your config in the `usertty()` driver.
   ([#3585](https://github.com/syslog-ng/syslog-ng/pull/3585))
 * `syslog-parser()`: add a new drop-invalid() option that allows the use of
   syslog-parser() in if statements. Normally a syslog-parser() injects an
   error message instead of failing.
   ([#3565](https://github.com/syslog-ng/syslog-ng/pull/3565))

## Bugfixes

 * date-parser: if the timestamp pattern did not covered a field (for example seconds) that field had undefined value
   
   The missing fields are initialized according to the following rules:
    1) missing all fields -> use current date
    2) only miss year -> guess year based on current year and month (current year, last year or next year)
    3) the rest of the cases don't make much sense, so zero initialization of the missing field makes sense. And the year is initialized to the current one.
   ([#3615](https://github.com/syslog-ng/syslog-ng/pull/3615))
 * Fix compilation issues on OpenBSD
   
   syslog-ng can now be compiled on OpenBSD.
   ([#3661](https://github.com/syslog-ng/syslog-ng/pull/3661))
 * loggen: debug message printed wrong plugin name (ssl-plugin instead of socket_plugin)
   ([#3624](https://github.com/syslog-ng/syslog-ng/pull/3624))
 * tls: fixup EOF detection issue in tls (before 3.0 version)
   
   syslog-ng error message:
   "I/O error occurred while reading; fd='13', error='Success (0)'"
   ([#3618](https://github.com/syslog-ng/syslog-ng/pull/3618))
 * kafka: the config() block couldn't contain option that is already a keyword in syslog-ng (example: retries)
   ([#3658](https://github.com/syslog-ng/syslog-ng/pull/3658))
 * templates: fixed error reporting when invalid templates were specified
   
   The `amqp()`, `file()` destination, `sql()`, `stomp()`, `pdbtool`, and
   `graphite()` plugins had template options that did not report errors at startup
   when invalid values were specified.
   ([#3660](https://github.com/syslog-ng/syslog-ng/pull/3660))

## Packaging

 * bison: minimum version of bison is bumped to 3.7.6
   ([#3547](https://github.com/syslog-ng/syslog-ng/pull/3547))
 * java-modules: the minimum version of gradle changed from 2.2 to 3.4
   ([#3645](https://github.com/syslog-ng/syslog-ng/pull/3645))
 * light: add to the release tarball
   ([#3613](https://github.com/syslog-ng/syslog-ng/pull/3613))

## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Andras Mitzki, Attila Szakacs, Balazs Scheidler,
Gabor Nagy, Janos SZIGETVARI, Laszlo Budai, Laszlo Szemere,
LittleFish33, László Várady, Ming Liu, Norbert Takacs, Peter Kokai,
Todd C. Miller, Yi Fan Yu, Zoltan Pallagi
