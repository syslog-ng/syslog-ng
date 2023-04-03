New metrics

`network()`, `syslog()`: TCP connection metrics

```
syslogng_socket_connections{id="tcp_src#0",driver_instance="afsocket_sd.(stream,AF_INET(0.0.0.0:5555))",direction="input"} 3
syslogng_socket_max_connections{id="tcp_src#0",driver_instance="afsocket_sd.(stream,AF_INET(0.0.0.0:5555))",direction="input"} 10
syslogng_socket_rejected_connections_total{id="tcp_src#0",driver_instance="afsocket_sd.(stream,AF_INET(0.0.0.0:5555))",direction="input"} 96
```

`internal()`: `internal_events_queue_capacity` metric

`syslog-ng-ctl healthcheck`: new healthcheck value `syslogng_internal_events_queue_usage_ratio`
