4.11.0
======

syslog-ng provides [RPM](https://github.com/syslog-ng/syslog-ng#rhel) and [DEB](https://github.com/syslog-ng/syslog-ng#debianubuntu) package repositories for Ubuntu, Debian, and RHEL, for both amd64 and arm64 architectures.

We also provide ready-to-test binaries in [Docker containers](https://hub.docker.com/r/balabit/syslog-ng/tags) based on the current stable Debian image.

For more details, visit our [Documentation Center](https://syslog-ng.github.io/)

## Highlights

  * `kafka-source()`: The new `kafka()` source can directly fetch log messages from the Apache Kafka message bus using the librdkafka client.

    It can fetch messages from explicitly named or wildcard-matching Kafka topics, and from a single partition, explicitly listed partitions, or all partitions of the selected topic(s). It supports the two main strategies — assign and subscribe — providing the flexibility to adapt to a wide range of Kafka setups and practical use cases, instead of forcing a single approach that may not fit all scenarios.\
    It can fetch and process messages from the Kafka broker using multiple workers(), which may further improve throughput, especially when the main worker can fetch messages at high speed.\
    For more [details](https://syslog-ng.github.io/admin-guide/060_Sources/038_Kafka/README) and for all available [options](https://syslog-ng.github.io/admin-guide/060_Sources/038_Kafka/001_Kafka_options), please see our documentation. ([#5564](https://github.com/syslog-ng/syslog-ng/pull/5564))

## Features

  * `cloud-auth`: Add generic OAuth2 authentication module

    A generic OAuth2 authentication module supporting client credentials flow with configurable authentication methods (HTTP Basic Auth or POST body credentials) has been added. The module is extensible via virtual methods, allowing future authenticators to inherit common OAuth2 token management logic.
    ([#5571](https://github.com/syslog-ng/syslog-ng/pull/5571))

  * `cloud-auth`, `grpc`: Add OAuth2 support for gRPC destinations

    The `cloud-auth()` plugin now supports gRPC destinations in addition to HTTP
    destinations. This enables OAuth2 token management for any gRPC-based
    destination driver (such as `bigquery()`, `opentelemetry()`, `loki()`, etc.).

    The implementation uses the same signal/slot pattern as HTTP.

    Example configuration:

    ``` config
    destination d_grpc {
      opentelemetry(
        url("example.com:443")
        cloud-auth(
          oauth2(
            client_id("client-id")
            client_secret("client-secret")
            token_url("https://auth.example.com/token")
            scope("api-scope")
          )
        )
      );
    }

    ```
    ([#5584](https://github.com/syslog-ng/syslog-ng/pull/5584))

  * `darwinosl()`: Added (fixed in an alternative way and enabled) the `log-fetch-limit()` option, which allows batched message forwarding in the log path.

    The `fetch-delay()` and `fetch-retry-delay()` options have both been renamed with the `log-` prefix; the old keywords are still supported but marked as deprecated.

    The enhanced internal loop for OSLog message processing now uses fewer resources during both processing and idle time.
    ([#5547](https://github.com/syslog-ng/syslog-ng/pull/5547))

  * network-load-balancer: add support for failover

    The confgen script for `network-load-balancer` now supports failover, generating the list of failover servers for each destination automatically.
    ([#5562](https://github.com/syslog-ng/syslog-ng/pull/5562))

  * `file()`, `wildcard-file()` source: Add `auto` follow-method() option

    A new follow-method() option, auto, has been added. In this automatic mode, syslog-ng OSE uses the following sequence to decide which method to use:

    - the `inotify` method is used automatically if the system supports it (and none of the IV_SELECT_POLL_METHOD or IV_EXCLUDE_POLL_METHOD environment variables are set); otherwise
    - the best available (or the IV_SELECT_POLL_METHOD or IV_EXCLUDE_POLL_METHOD forced) `system` (ivykis) poll method of the platform is used; if none is available,
    - the old `poll` method is used

    Unfortunately, for backward compatibility reasons, the new `auto` option could not become the default at this time, but for new configurations, we recommend using monitor-method("auto") and follow-method("auto") to achieve the best available performance on the given platform.
    ([#5602](https://github.com/syslog-ng/syslog-ng/pull/5602))

  * `network()`/`syslog()`: Add `extended-key-usage-verify()` option to TLS sources and destinations
    ([#5588](https://github.com/syslog-ng/syslog-ng/pull/5588))

  * Number parsing: Improve @NUMBER@ / @DOUBLE@ support for signed numbers

    When parsing numbers using the @NUMBER@ or @DOUBLE@ pattern parsers, certain signs were not allowed, causing PatternDB to reject properly formatted numbers. Explicit `+` signs at the beginning of a number or in the exponent part were rejected, and a `-` sign before a hexadecimal value was also not accepted. ([#5620](https://github.com/syslog-ng/syslog-ng/pull/5620))

  * `elasticsearch`, `opensearch`: Update SCL to support data streams

    - elasticsearch-http() and opensearch() now support data streams when using the op_type("create") parameter
    - the type("") parameter can now be omitted, and its value is silently ignored

    ([#5586](https://github.com/syslog-ng/syslog-ng/pull/5586))

## Bugfixes

  * `json-parser`: Fixed quoting of JSON array elements containing commas.
    ([#5604](https://github.com/syslog-ng/syslog-ng/pull/5604))

  * `transport(proxied-tcp)`: Fixed an internal assertion–caused crash when a Proxy Protocol v2 message was received with a LOCAL command that was not handled previously. LOCAL commands are now treated as health check messages for both v1 and v2 proxy messages — accepted but dropped — as the [protocol definition](www.haproxy.org/download/3.0/doc/proxy-protocol.txt) specifies.
    ([#5608](https://github.com/syslog-ng/syslog-ng/pull/5608))

  * `afsocket-source`: Fixed two `keep-alive()` configuration reload–related issues.
    First, when a reload switched from `keep-alive(yes)` to `keep-alive(no)` with `so-reuseport(no)`, the newly loaded configuration’s socket instance could fail to open.
    Second, in case of an error in the new configuration, the reload could fail to properly restore the previous connection state.
    ([#5552](https://github.com/syslog-ng/syslog-ng/pull/5552))

  * `log-transport-tls`: TLS-related macros, such as ${tls.x509_cn}, were not set in the first log message.
    ([#5603](https://github.com/syslog-ng/syslog-ng/pull/5603))

  * stats: Fixed stats reset memory counter underflow.
    ([#5563](https://github.com/syslog-ng/syslog-ng/pull/5563))

  * `afsocket-source`: Added a workaround for an issue where a reload switches from `keep-alive(yes)` to `keep-alive(no)` with `so-reuseport(no)`, causing the newly loaded configuration’s socket instance to fail to open.
    ([#5552](https://github.com/syslog-ng/syslog-ng/pull/5552))

## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

AshFungor, Bálint Horváth, Peter Czanik (CzP), davidtosovic-db, egglessness,
Daniele Ferla, Hofi, Tamas Pal, Romain Tartière, Fᴀʙɪᴇɴ Wᴇʀɴʟɪ
