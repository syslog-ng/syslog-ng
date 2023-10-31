`tls()` in `udp()`/`tcp()`/`network()` and `syslog()` drivers: add support
for a new `http()` compatible ssl-version() option. This makes the TLS
related options for http() and other syslog-like drivers more similar. This
requires OpenSSL 1.1.0.
