**Receiving OpenTelemetry (OTLP) messages**

The `opentelemetry()` source is now available to receive **OTLP/gRPC** messages.

syslog-ng accepts [logs](https://github.com/open-telemetry/opentelemetry-proto/blob/v0.20.0/opentelemetry/proto/logs/v1/logs.proto), [metrics](https://github.com/open-telemetry/opentelemetry-proto/blob/v0.20.0/opentelemetry/proto/metrics/v1/metrics.proto) and [traces](https://github.com/open-telemetry/opentelemetry-proto/blob/v0.20.0/opentelemetry/proto/trace/v1/trace.proto).

The incoming fields are not available through syslog-ng log message name-value pairs for the user by default.
This is useful for forwarding functionality (the upcoming `opentelemetry()` destination will be able to access
and format them). If such functionality is required, you can configure the `opentelemetry()` parser, which maps
all the fields with some limitations.

The behavior of the parser is the following:

The name-value pairs always start with `.otel.` prefix. The type of the message is stored in `.otel.type`
(possible values: `log`, `metric` and `span`). The `resource` info is mapped to `.otel.resource.<...>`
(e.g.: `.otel.resource.dropped_attributes_count`, `.otel.resource.schema_url` ...), the `scope` info 
is mapped to `.otel.scope.<...>` (e.g.: `.otel.scope.name`, `.otel.scope.schema_url`, ...).

The fields of log records are mapped to `.otel.log.<...>` (e.g. `.otel.log.body`, ` .otel.log.severity_text`, ...).

The fields of metrics are mapped to `.otel.metric.<...>` (e.g. `.otel.metric.name`, `.otel.metric.unit`, ...),
the type of the metric is mapped to `.otel.metric.data.type` (possible values: `gauge`, `sum`, `histogram`,
`exponential_histogram`, `summary`) with the actual data mapped to `.otel.metric.data.<type>.<...>`
(e.g.: `.otel.metric.data.gauge.data_points.0.time_unix_nano`, ...).

The fields of traces are mapped to `.otel.span.<...>` (e.g. `.otel.span.name`, `.otel.span.trace_state`, ...).

`repeated` fields are given an index (e.g. `.otel.span.events.5.time_unix_nano`).

The mapping of [`AnyValue`](https://github.com/open-telemetry/opentelemetry-proto/blob/v0.20.0/opentelemetry/proto/common/v1/common.proto#L28) type fields is limited.
`string`, `bool`, `int64`, `double` and `bytes` values are mapped with the respective syslog-ng name-value type
(e.g. `.otel.resource.attributes.string_key` => `string_value`), however `ArrayValue` and `KeyValueList` types
are stored serialized with `protobuf` type. `protobuf` and `bytes` types are not directly available for the
user, unless an explicit type cast is added (e.g. `"bytes(${.otel.log.span_id})"`) or `--include-bytes` is passed
to name-value iterating template functions (e.g. `$(format-json .otel.* --include-bytes)`, which will base64
encode the bytes content).

Three authentication methods are available in the `auth()` block: `insecure()` (default), `tls()` and `alts()`.
`tls()` accepts the `key-file()`, `cert-file()`, `ca-file()` and `peer-verify()` (possible values:
`required-trusted`, `required-untrusted`, `optional-trusted` and `optional-untrusted`) options.
[ALTS](https://grpc.io/docs/languages/cpp/alts/) is a simple to use authentication, only available within Google's infrastructure.
