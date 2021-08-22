`mqtt()`: TLS and WebSocket Secure support

The MQTT destination now supports TLS and WSS.

Example config:
```
mqtt(
  address("ssl://localhost:8883")
  topic("syslog/$HOST")
  fallback-topic("syslog/fallback")

  tls(
    ca-file("/path/to/ca.crt")
    key-file("/path/to/client.key")
    cert-file("/path/to/client.crt")
    peer-verify(yes)
  )
);
```
