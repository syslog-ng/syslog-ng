`network`, `syslog`: `extended-key-usage-verify()` in source `tls()` options

`extended-key-usage-verify()` can now be used in the `tls()` options of sources too, to require
the `clientAuth` extended key usage on client certificates (it was destination-only).
