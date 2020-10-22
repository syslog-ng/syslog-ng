network: fix TLS certificate hostname verification when using `failover()` servers

For TLS certificate hostname verification, the certificate's hostname needs to be compared to the configured hostname
of the primary and each failover server. syslog-ng used always the primary server's name incorrectly.
