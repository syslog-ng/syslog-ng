`darwinosl()`: Added (fixed in an alternative way and enabled) the `log-fetch-limit()` option, which allows batched message forwarding in the log path.

The `fetch-delay()` and `fetch-retry-delay()` options have both been renamed with the `log-` prefix; the old keywords are still supported but marked as deprecated.

The enhanced internal loop for OSLog message processing now uses fewer resources during both processing and idle time.
