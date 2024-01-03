`opentelemetry()`, `syslog-ng-otlp()` destinations: Added a new `batch-bytes()` option.

This option lets the user limit the bytes size of a batch. As there is a
default 4 MiB batch limit by OTLP, it is necessary to keep the batch size
smaller, but it would be hard to configure without this option.

Please note that the batch can be at most 1 message larger than the set
limit, so consider this when setting this value.

The default value is 4 MB, which is a bit below 4 MiB.

The calculation of the batch size is done before compression, which is
the same as the limit is calculated on the server.

Example config:
```
  syslog-ng-otlp(
    url("localhost:12345")
    workers(16)
    log-fifo-size(1000000)

    batch-timeout(5000) # ms
    batch-lines(1000000) # Huge limit, batch-bytes() will limit us sooner

    batch-bytes(1MB) # closes and flushes the batch after the last message pushed it above the 1 MB limit
    # not setting batch-bytes() defaults to 4 MB, which is a bit below the default 4 MiB limit
  );
```
