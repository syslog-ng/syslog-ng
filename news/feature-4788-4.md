`opentelemetry()`: Added support for single depth `Array`s and `KeyValueList`s of strings.

The `opentelemetry()` source, parser and destination and the `syslog-ng-otlp()`
source and destination now supports the `list` and the newly added `kvlist`
syslog-ng internal types. The former resolves to an `Array` of strings, the latter
resolves to a `KeyValueList` of strings.

Please note that this still does not provide full `Array` and `KeyValueList` support.
If the `opentelemetry()` parser receives an `Array` or `KeyValueList` with different
value types, syslog-ng will not extract those values and the `protobuf` type will be used.
