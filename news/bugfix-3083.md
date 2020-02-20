http: Fixed handling of ssl-version() option, which was ignored before.

Prior this fix, these values of `ssl-version` in http destination were ignored by syslog-ng:
`tlsv1_0`, `tlsv1_1`, `tlsv1_2`, `tlsv1_3`.
