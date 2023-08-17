**Sending messages between syslog-ng instances via OTLP/gRPC**

The `syslog-ng-otlp()` source and destination helps to transfer the internal representation
of a log message between syslog-ng instances. In contrary to the `syslog-ng()` (`ewmm()`)
drivers, `syslog-ng-otlp()` does not transfer the messages on simple TCP connections, but uses
the OpenTelemetry protocol to do so.

It is easily scalable (`workers()` option), uses built-in application layer acknowledgement,
out of the box supports google service authentication (ADC or ALTS), and gives the possibility
of better load balancing.

The performance is currently similar to `ewmm()` (OTLP is ~30% quicker) but there is a source
side limitation, which will be optimized. We measured 200-300% performance improvement with a
PoC optimized code using multiple threads, so stay tuned.

Note: The `syslog-ng-otlp()` source is only an alias to the `opentelemetry()` source.
This is useful for not needing to open different ports for the syslog-ng messages and other
OpenTelemetry messages. The syslog-ng messages are marked with a `@syslog-ng` scope name and
the current syslog-ng version as the scope version. Both sources will handle the incoming
syslog-ng messages as syslog-ng messages, and all other messages as simple OpenTelemetry
messages.
