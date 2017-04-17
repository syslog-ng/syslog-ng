3.9.1

<!-- Tue, 20 Dec 2016 11:59:09 +0100 -->

# Features
 * Improve parsing performance in case of keep-timestamp(no), earlier the
   timestamp was parsed and then dropped, now we don't parse it, which is a
   2x performance improvement in reception speed.

 * TLS based transports will publish the peer's certificate in a set of
   name-value pairs, as follows:

    * .tls.x509_cn - X.509 common name
    * .tls.x509_o  - X.509 organization string
    * .tls.x509_ou - X.509 organizational unit

 * Improve performance of the tcp() source, due to a bug, syslog-ng
   attempted to apply position tracking to messages coming over a TCP
   transport, which is used for file position tracking and causing
   performance degradation. This bug is fixed, causing performance to be
   increased. (#1195)

 * Make it possible to configure the listen-backlog() for any stream based
   transports (unix-stream and tcp).  Earlier this was hard-wired at 256
   connections, now can be tuned using an option. For example:

     tcp(port(6514) listen-backlog(2048));

 * Add a groupunset() rewrite rule that pairs up with groupset() but instead
   of setting values it unsets them. (#1235)

 * Add support for Elastic Shield (#1228) and SearchGuard (#1223)

 * kv-parser() is now able to cope with unquoted values with an embedded
   space in them, it also trims whitespace from keys/values and is in
   general more reliable in extracting key-value pairs from arbitrary log
   messages.

 * Improve performance for java based destinations. (#1243)

 * Add prefix() option to add-contextual-data()


# Bugfixes
 * Fix a potential crash in the file destination, in case it is a template
   based filename and time-reap() is elapsed. (#1183)

 * Fix a potential ACK problem within syslog-ng that can cause input windows
   to overflow queue sizes over time, effectively causing message drops that
   shouldn't occur. (#1230)

 * Fix a heap corruption bug in the DNS cache, in case the maximum number of
   DNS cache entries is reached. (#1218)

 * Fix timestamp for suppression messages. (#1233)

 * Fix add-contextual-data() to support CRLF line endings in its CSV input
   files.

 * Fixed key() option parsing in riemann() destinations.

 * Find libsystemd-journal related functions in both libsystemd-journal.so
   and libsystemd.so, as recent systemd versions bundled all systemd
   related libs into the same library.

 * Fixed the build-time detection of system-wide installed librabbitmq,
   libmongoc and libcap.

 * Fix the file source to repeatedly check for unexisting files, as a bug
   caused syslog-ng to stop after two attempts previously. (#841)

 * The performance testing tool "loggen" crashed if it was used to generate
   messages on multiple threads over TLS. This was now fixed. (#1182)

 * Fix an issue in the syslog-parser() parser, so that timestamps parsed
   earlier in the log path are properly overwritten.  Earlier a time-zone
   setting may have remained in the timestamp in case the first timestamp
   did contain a timezone and then the one parsed by syslog-parser() didn't.
   (#1206)

 * Due to a compilation issue, tcp-keepalive-time(), tcp-keepalive-intvl() and
   tcp-keepalive-probes() were not working, now they are again. (#1214)

 * The --disable-shm-counters option is now passed to mongo-c-driver to work
   around a minor security issue (#1219).

     https://jira.mongodb.org/plugins/servlet/mobile#issue/CDRIVER-1691/comment/1405406

 * Fix compilation issues on FreeBSD. (#1252)

 * Add support to month names in all caps in syslog timestamps. At least one
   device seems to generate these. (#1263)

 * The options() option to java destination can now accept numbers and not
   just strings.

 * Fix a memory leak in the java destination driver, that may affect java
   based destinations like ElasticSearch, Kafka & HDFS.

# Other changes

 * HDFS was updated to 2.7.3

 * Elasticsearch was updated to 2.4.0

 * Support was added for OpenSSL 1.1.x (#1281)

# Notes to the Developers

 * We started to standardize our tests on the criterion unit testing
   framework, please submit all new tests using this framework. Patches to
   convert existing ones are also welcome.
   https://github.com/Snaipe/Criterion

 * We also added a configuration file for astyle and accompanying make
   targets to check/reformat the source code to meet syslog-ng's style.

 * debian/ directory has been removed from the "master" branch and is now
   maintained in a separate "release" branch.

# Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Lászlo Várady, 0xddaa, Balázs Scheidler, Tamás Nagy, László Budai,
Fabien Wernli, Viktor Juhász, Kyle Manna, Michael Wimpy, Noémi Ványi,
Attila Szalay, Tibor Bodnár, Zoltán Pallagi
