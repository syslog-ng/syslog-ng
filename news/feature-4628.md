Add `ignore-hostname-mismatch` as a new flag to `ssl-options()`: by
specifying `ignore-hostname-mismatch`, you can ignore the subject name of a
certificate during the validation process. This means that syslog-ng will
only check if the certificate itself is trusted by the current set of trust
anchors (e.g. trusted CAs) ignoring the mismatch between the targeted
hostname and the certificate subject.
