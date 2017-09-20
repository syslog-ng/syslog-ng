3.12.1

<!-- Tue, 19 Sep 2017 12:59:40 +0200 -->

# Features

* HDFS: support macro in filename (#1638)
* HDFS: add append support (#1675)
* Java: allow to use sequence numbers in templates (#1628)
* TLS improvements (#1603, #1636)
    - Add **PKCS 12** support with the new `pkcs12-file()` TLS option
    - startup time `ssl-options()` and `peer-verify()` check
    - startup time `key_file`, `cert_file`, `ca_dir`, `crl_dir` and `cipher_suite` check
    - **ECDH  cipher support** (OpenSSL 1.0.1, 1.0.2, 1.1.0) with the `ecdh-curve-list()` option (only available >= 1.0.2)
        - for < 1.0.2, a hard-coded curve is used
        - for >= 1.0.2, automatic curve selection is used (the `ecdh-curve-list()` option can restrict this list)
    - **DH cipher support** with the `dhparam-file()` option
        - if the option is not specified, fallback RFC 3526 parameters are used
    - minor fixes
* `stdin()` source driver (#1605)
* Implement `read_old_records` option for systemd-journal source (#1642)
* Add tags-parser: a new module to parse $TAGS values (#1658)
* Add a Windows eventlog parser scl module (#1572)
* Add XML parser module (#1659, #1684)

# Bugfixes

* Fix cannot parse ipv6 into hostname (#1617)
* Speedup add-contextual-data by making ordering optional (#1645)
* Fix `monitor-method()` option not working for `wildcard-file()` source (#1651)
* Sanitize SDATA keys in syslog-protocol messages to avoid generating non-valid messages (#1650, #1654)
* Fix memory leaks reported using Valgrind (#1649)
* Fix memory leak related to cloning pipes and reload (#1647)
* Fix getent protocol number returns incorrect value (#1665)
* Fix elasticsearch2 destination flush mechanism (#1668)
* Fix file destination related memory leak (#1685)
* Fix a possible memory leak around affile destination (#1685)

# Other changes

* Improve syslog-ng debun functionality (#1633, #1641, #1663)
* Java: allow to set JVM options form global syslog-ng options (#1639)
* Do steps towards Python 3 support:
    * Fix string compatibility for Python 3 (#1632)
    * Improve Python version auto detection (#1660)
* HTTP destination: display verbose logs on debug level (#1526)
* Improvements for Solaris packing (#1664)

# Notes to the Developers

* Update internal RabbitMQ (#1662)
* Update internal ivykis to v0.42 (#1566)
* Fix Travis and test related issues (#1566, #1644, #1674)
* Update docker images (#1637)
* Fix some clang compile errors (#1662)

# Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:
Andras Mitzki, Antal Nemes, Attila Szalay, Balazs Scheidler, Gabor Nagy,
Gergely Orosz, Janos Szigetvari, Laszlo Budai, Laszlo Varady, Mate Farkas,
Marton Suranyi, Peter Kokai, Szilard Pfeiffer, Tamas Nagy, Zoltan Pallagi.

