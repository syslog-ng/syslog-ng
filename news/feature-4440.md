`network()`, `syslog()`, `file()`, `http()`: new byte-based metrics for incoming/outgoing events

These metrics show the serialized message sizes (protocol-specific header/framing/etc. length is not included).

```
syslogng_input_event_bytes_total{id="s_network#0",driver_instance="tcp,127.0.0.1"} 1925529600
syslogng_output_event_bytes_total{id="d_network#0",driver_instance="tcp,127.0.0.1:5555"} 565215232
syslogng_output_event_bytes_total{id="d_http#0",driver_instance="http,http://127.0.0.1:8080/"} 1024
```
