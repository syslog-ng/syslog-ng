`metrics`: new network (TCP, UDP) metrics are available on stats level 1

```
# syslog-ng-ctl stats prometheus

syslogng_socket_receive_buffer_used_bytes{id="#anon-source0#3",direction="input",driver_instance="afsocket_sd.udp4"} 0
syslogng_socket_receive_buffer_max_bytes{id="#anon-source0#3",direction="input",driver_instance="afsocket_sd.udp4"} 268435456
syslogng_socket_receive_dropped_packets_total{id="#anon-source0#3",direction="input",driver_instance="afsocket_sd.udp4"} 619173

syslogng_socket_connections{id="#anon-source0#0",direction="input",driver_instance="afsocket_sd.(stream,AF_INET(0.0.0.0:2000))"} 1
```
