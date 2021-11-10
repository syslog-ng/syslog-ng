network: add support for PROXY header before TLS payload

This PR adds a new transport method `proxied-tls-passthrough` which is capable of detecting the [PROXY header](http://www.haproxy.org/download/1.8/doc/proxy-protocol.txt) before the TLS payload. Loggen has been updated with the `--proxied-tls-passthrough` option for testing purposes.


```
source s_proxied_tls_passthrough{
  network(
    port(1234)
    transport("proxied-tls-passthrough"),
    tls(
      key-file("/path/to/server_key.pem"),
      cert-file("/path/to/server_cert.pem"),
      ca-dir("/path/to/ca/")
    )
  );
};
```
