gRPC based destinations: Added `response-action()` option

With this option, it is possible to fine tune how syslog-ng
behaves in case of different gRPC results.

Supported by the following destination drivers:
  * `opentelemetry()`
  * `loki()`
  * `bigquery()`
  * `clickhouse()`
  * `google-pubsub-grpc()`

Supported gRPC results:
  * ok
  * unavailable
  * cancelled
  * deadline-exceeded
  * aborted
  * out-of-range
  * data-loss
  * unknown
  * invalid-argument
  * not-found
  * already-exists
  * permission-denied
  * unauthenticated
  * failed-precondition
  * unimplemented
  * internal
  * resource-exhausted

Supported actions:
  * disconnect
  * drop
  * retry
  * success

Usage:
```
google-pubsub-grpc(
  project("my-project")
  topic("my-topic")
  response-action(
    not-found => disconnect
    unavailable => drop
  )
);
```
