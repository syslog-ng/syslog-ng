network drivers: add TLS keylog support
syslog-ng dumps TLS secrets for a given source/destination, which can be used for debugging purposes to decrypt data with, for example, Wireshark.
**This should be used for debugging purposes only!**.
```
source tls_source{
  network(
      port(1234)
      transport("tls"),
      tls(
        key-file("/path/to/server_key.pem"),
        cert-file("/path/to/server_cert.pem"),
        ca-dir("/path/to/ca/")
        keylog-file("/path/to/keylog_file")
        )
      );
};
```
