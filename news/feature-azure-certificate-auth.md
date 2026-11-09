`cloud-auth`: Azure authentication now supports PEM RSA certificate credentials through `cert()` and `private-key()`,
with an optional `private-key-passphrase()` for encrypted keys, as an alternative to `app-secret()`.
Upload the matching public certificate to the Entra app registration; the private key remains on the syslog-ng host.
