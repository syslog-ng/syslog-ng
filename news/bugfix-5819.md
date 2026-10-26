`tls`: Fix loading password-protected `pkcs12-file()` bundles without a MAC on older OpenSSL versions.

The credential-store password now works for these bundles with OpenSSL 1.1.1 and 3.0 as well as newer
OpenSSL 3.x and 4.x releases. Existing MACs are still verified, and the original bundle is not modified.
