`afsocket`: Socket options, such as ip-ttl() or tcp-keepalive-time(), are
traditionally named by their identifier defined in socket(7) and unix(7) man
pages.  This was not the case with the pass-unix-credentials() option, which
- unlike other similar options - was also possible to set globally.

A new option called so-passcred() is now introduced, which works similarly
how other socket related options do, which also made possible a nice code
cleanup in the related code sections.  Of course the old name remains
supported in compatibility modes.

The PR also implements a new source flag `ignore-aux-data`, which causes
syslog-ng not to propagate transport-level auxiliary information to log
messages.  Auxiliary information includes for example the pid/uid of the
sending process in the case of UNIX based transports, OR the X.509
certificate information in case of SSL/TLS encrypted data streams.

By setting flags(ignore-aux-data) one can improve performance at the cost of
making this information unavailable in the log messages received through
affected sources.
