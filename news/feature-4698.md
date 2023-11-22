### Sending log messages to OpenObserve
The `openobserve-log()` destination feeds OpenObserve via the [JSON API](https://openobserve.ai/docs/api/ingestion/logs/json/).

Example config:
```
openobserve-log(
    url("http://openobserve-endpoint")
    port(5080)
    stream("default")
    user("root@example.com")
    password("V2tsn88GhdNTKxaS")
);
```

