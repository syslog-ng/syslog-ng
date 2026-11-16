`tls`: Fixed certificate hostname verification accepting names with an embedded NUL byte.
A certificate subjectAltName or common name such as `good.example.com\0.evil.example.net`
was compared as a C string and matched only the part before the NUL, letting a certificate
issued for a different name pass the configured hostname check.
