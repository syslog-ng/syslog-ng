`metrics-probe()`: Fixed a crash.

This crash occurred when a `metrics-probe()` instance was used in multiple source threads,
like a `network()` source with multiple connections.
