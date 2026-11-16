`tls`: `pkcs12-file()` now supports password-protected PKCS#12 files. Like for `key-file()`, the password is
added with `syslog-ng-ctl credentials add --id=<pkcs12-file path> --secret=<password>`; the source starts
once it is available.
