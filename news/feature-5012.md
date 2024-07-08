`opentelemetry()`, `loki()`, `bigquery()`: Added `headers()` option

Enables adding headers to RPC calls.

Example config:
```
opentelemetry(
  ...
  headers(
    "my_header" = "my_value"
  )
);
```
