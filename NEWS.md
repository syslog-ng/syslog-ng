3.34.1
======

## Highlights

 * `regexp-parser()`: new parser that can parse messages with regular expressions

   Example:
   ```
   regexp-parser(
     template("${MESSAGE}")
     prefix(".regexp.")
     patterns("(?<DN>foo)", "(?<DN>ball)")
   );
   ```

   `regexp-parser()` can be used as an intuitive replacement for regexp filters
   that had their `store-matches` flag set in order to save those matches.

   ([#3702](https://github.com/syslog-ng/syslog-ng/pull/3702))

 * `redis()`: `workers()` and batching support

   The Redis driver now support the `workers()` option, which specifies the
   number of parallel workers, and the `batch-lines()` option.

   This could drastically increase the throughput of the Redis destination driver.

   Example:
   ```
   redis(
       host("localhost")
       port(6379)
       command("HINCRBY", "hosts", "$HOST", "1")
       workers(8)
       batch-lines(100)
       batch-timeout(10000)
       log-fifo-size(100000)
   );
   ```
   ([#3732](https://github.com/syslog-ng/syslog-ng/pull/3732),
    [#3745](https://github.com/syslog-ng/syslog-ng/pull/3745))

 * `mqtt()`: TLS and WebSocket Secure support

   The MQTT destination now supports TLS and WSS.

   Example config:
   ```
   mqtt(
     address("ssl://localhost:8883")
     topic("syslog/$HOST")
     fallback-topic("syslog/fallback")

     tls(
       ca-file("/path/to/ca.crt")
       key-file("/path/to/client.key")
       cert-file("/path/to/client.crt")
       peer-verify(yes)
     )
   );
   ```
   ([#3747](https://github.com/syslog-ng/syslog-ng/pull/3747))

## Features

 * `system()` source: added support for NetBSD
   ([#3761](https://github.com/syslog-ng/syslog-ng/pull/3761))

 * `stats`: new statistics counter

   The following statistics are now available for the HTTP destination, and
   other file and network based sources/destinations:

   - `msg_size_max`/`msg_size_avg`: Shows the largest/average message size of the given source/destination that has
     been measured so far.
   - `batch_size_max`/`batch_size_avg`: When batching is enabled, then this shows the
     largest/average batch size of the given source/destination that has been measured so far.

   - `eps_last_1h`, `eps_last_24h`, `eps_since_start`: Events per second, measured for the last hour,
     for the last 24 hours, and since syslog-ng startup, respectively.

   Notes:
   - Message sizes are calculated from the incoming raw message length on the source side, and from the outgoing
     formatted message length on the destination side.
   - EPS counters are just approximate values, they are updated every minute.
   ([#3753](https://github.com/syslog-ng/syslog-ng/pull/3753))

 * `mqtt()`: username/password authentication

   Example config:
   ```
   mqtt(
     address("tcp://localhost:1883")
     topic("syslog/messages")
     username("user")
     password("passwd")
   );
   ```

   Note: The password is transmitted in cleartext without using `ssl://` or `wss://`.
   ([#3747](https://github.com/syslog-ng/syslog-ng/pull/3747))

 * `mqtt()`: new option `http-proxy()` for specifying HTTP/HTTPS proxy for WebSocket connections
   ([#3747](https://github.com/syslog-ng/syslog-ng/pull/3747))

 * `syslog-ng-ctl`: new flag for pruning statistics

   `syslog-ng-ctl stats --remove-orphans` can be used to remove "orphaned" statistic counters.
   It is useful when, for example, a templated file destination (`$YEAR.$MONTH.$DAY`) produces a lot of stats,
   and one wants to remove those abandoned counters occasionally/conditionally.
   ([#3760](https://github.com/syslog-ng/syslog-ng/pull/3760))

 * `disk-buffer()`: added a new option to reliable disk-buffer: `qout-size()`.

   This option sets the number of messages that are stored in the memory in addition
   to storing them on disk. The default value is 1000.

   This serves performance purposes and offers the same no-message-loss guarantees as
   before.

   It can be used to maintain a higher throughput when only a small number of messages
   are waiting in the disk-buffer.
   ([#3754](https://github.com/syslog-ng/syslog-ng/pull/3754))

## Bugfixes

 * `network(), syslog()`: fixed network sources on NetBSD

   On NetBSD, TCP-based network sources closed their listeners shortly after
   startup due to a non-portable TCP keepalive setting. This has been fixed.
   ([#3751](https://github.com/syslog-ng/syslog-ng/pull/3751))

 * `disk-buffer()`: fixed a very rare case, where the reliable disk-buffer never resumed
   after triggering `flow-control`.
   ([#3752](https://github.com/syslog-ng/syslog-ng/pull/3752))

 * `disk-buffer()`: fixed a rare memory leak that occurred when `mem-buf-length()`
   or `mem-buf-size()` was configured incorrectly
   ([#3750](https://github.com/syslog-ng/syslog-ng/pull/3750))

 * `redis()`: fixed command errors that were not detected and marked as successful delivery
   ([#3748](https://github.com/syslog-ng/syslog-ng/pull/3748))

## Notes to developers

 * Light framework: new proxy-related options are supported with loggen:
   `--proxy-src-ip`, `--proxy-dst-ip`, `--proxy-src-port`, `--proxy-dst-port`
   ([#3766](https://github.com/syslog-ng/syslog-ng/pull/3766))

 * `log-threaded-dest`: descendant drivers from LogThreadedDest no longer inherit
   batch-lines() and batch-timeout() automatically. Each driver have to opt-in for
   these options with `log_threaded_dest_driver_batch_option`.

   `log_threaded_dest_driver_option` has been renamed to `log_threaded_dest_driver_general_option`,
   and `log_threaded_dest_driver_workers_option` have been added similarly to the
   batch-related options.
   ([#3741](https://github.com/syslog-ng/syslog-ng/pull/3741))

## Other changes

 * `disk-buffer()`: performance improvements

   Based on our measurements, the following can be expected compared to the previous syslog-ng release (v3.33.1):
   - non-reliable disk buffer: up to 50% performance gain;
   - reliable disk buffer: up to 80% increase in performance.

   ([#3743](https://github.com/syslog-ng/syslog-ng/pull/3743), [#3746](https://github.com/syslog-ng/syslog-ng/pull/3746),
   [#3754](https://github.com/syslog-ng/syslog-ng/pull/3754), [#3756](https://github.com/syslog-ng/syslog-ng/pull/3756),
   [#3757](https://github.com/syslog-ng/syslog-ng/pull/3757))

 * `disk-buffer()`: the default value of the following options has been changed for performance reasons:
     - `truncate-size-ratio()`: from 0.01 to 0.1 (from 1% to 10%)
     - `qout-size()`: from 64 to 1000 (this affects only the non-reliable disk buffer)
   ([#3757](https://github.com/syslog-ng/syslog-ng/pull/3757))

 * `kafka-c()`: `properties-file()` option is removed

   Please list librdkafka properties in the `config()` option in syslog-ng's configuration.
   See librdkafka configuration [here](https://github.com/edenhill/librdkafka/blob/master/CONFIGURATION.md).
   ([#3704](https://github.com/syslog-ng/syslog-ng/pull/3704))

## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Andras Mitzki, Attila Szakacs, Balazs Scheidler, Balázs Barkó,
Benedek Cserhati, Fabrice Fontaine, Gabor Nagy, Laszlo Szemere,
LittleFish33, László Várady, Norbert Takacs, Parrag Szilárd,
Peter Czanik, Peter Kokai, Zoltan Pallagi
