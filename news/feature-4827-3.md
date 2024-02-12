gRPC based drivers: Added `channel-args()` option.

Affected drivers are:
 * `bigquery()` destination
 * `loki()` destination
 * `opentelemetry()` source and destination
 * `syslog-ng-otlp()` source and destination

The `channel-args()` option accepts name-value pairs and sets channel arguments
defined in https://grpc.github.io/grpc/core/group__grpc__arg__keys.html

Example config:
```
  opentelemetry(
    channel-args(
      "grpc.loadreporting" => 1
      "grpc.minimal_stack" => 0
    )
  );
```
