network plugins: better timer defaults for TCP keepalive

From now on, syslog-ng uses the following defaults for TCP keepalive:
- `tcp-keepalive-time()`: 60
- `tcp-keepalive-intvl()`: 10
- `tcp-keepalive-probes()`: 6

Note: `so-keepalive()` is enabled by default.
