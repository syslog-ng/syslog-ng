`sumologic-http()` improvements

Breaking change: `sumologic-http()` originally sent incomplete messages (only the `$MESSAGE` part) to Sumo Logic by default.
The new default is a JSON object, containing all name-value pairs.

To override the new message format, the `template()` option can be used.

`sumologic-http()` now supports batching, which is enabled by default to increase the destination's performance.

The `tls()` became optional, Sumo Logic servers will be verified using the system's certificate store by default.
