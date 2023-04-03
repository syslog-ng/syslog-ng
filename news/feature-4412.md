`network` source: During a TLS handshake, syslog-ng now automatically sets the
`certificate_authorities` field of the certificate request based on the `ca-file()`
and `ca-dir()` options. The `pkcs12-file()` option already had this feature.
