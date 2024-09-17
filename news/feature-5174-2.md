`bigquery()`: Added `auth()` options.

Similarly to other gRPC based destination drivers, the `bigquery()`
destination now accepts different authentication methods, like
`adc()`, `alts()`, `insecure()` and `tls()`.

```
bigquery (
    ...
    auth(
        tls(
            ca-file("/path/to/ca.pem")
            key-file("/path/to/key.pem")
            cert-file("/path/to/cert.pem")
        )
    )
);
```
