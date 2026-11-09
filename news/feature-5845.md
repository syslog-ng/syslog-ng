`cloud-auth`: Azure authentication now supports PEM RSA certificate credentials through `cert()` and `private-key()`,
with an optional `private-key-passphrase()` for encrypted keys, as an alternative to `app-secret()`.
Upload the matching public certificate to the Entra app registration; the private key remains on the syslog-ng host.

The generic `oauth2()` authenticator gained the same capability through `auth-method(private-key-jwt)` together
with `cert()`, `private-key()` and the optional `private-key-passphrase()` options. The client authenticates with an
RFC 7523 JWT bearer client assertion signed by the private key instead of `client-secret()`.
