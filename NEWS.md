3.7.2
=====

<!-- Mon, 26 Oct 2015 11:18:07 +0100 -->

This is the first maintenance release for the 3.7.x series.

Changes compared to 3.7.1:

Improvements
------------

 * Added mbox() source.
   This source can be used to fetch emails from local mbox files:
   source { mbox("/var/spool/mail/root"); };
   This will fetch root emails and parse them into a multiline $MSG.
   Original implementation by Fabien Wernli, I only converted it into
   an SCL.

 * It is possible to append dynamically options into SCL blocks from now.

 * `concurrent_request` option added to ElasticSearch with default value 1.

 * In elasticsearch destinaton, message_template() argument renamed to 
   template().

 * SCL added to every Java module (ElasticSearch, Kafka, HDFS).

 * Linux Audit Parser added for parsing key-value pairs produced by 
   the Linux Audit subsystem.

 * HTTP destination is now able to receive HTTP method as an option.
   All the supported methods are available
   (POST, PUT, HEAD, OPTIONS, DELETE, TRACE, GET). 


Fixes
-----

 * In some circumstances syslog-ng mod-journal re-read every already
   processed messages.

 * When syslog-ng got a reload and the reload process done within 1 second then
   mafter the reload, syslog-ng stop generating mark-messages.

 * When initialization of a network destination in syslog-ng failed (eg. due to
   DNS resolution failure) we didn't create a queue which caused message loss.

 * syslog-ng segfaulted on TLS errors when wrong certs was provided
  (eg.: CA cert with the cert-file directive instead of the server cert).

 * Fixed a continuous spinning case in the file driver, when the
   destination file is a device (e.g. /dev/stdout).

 * A memory leak in around template functions in grammar fixed.

 * Fixed Python3 support.

 * Fixed Python GIL issue in python destination.

 * From now, instead of skipping doc/ alltogether when ENABLE_MANPAGES is
   not set, only skip the actual man pages, but handle the rest properly.

 *  Allow overriding the python setup.py options.

    When installing the python modules, allow overriding the options. This
    is useful for distributions that want to pass extra options. For
    example, on Debian, we want --install-layout="deb" instead of the
    --prefix and --root options.

    With this change, the previous behaviour remains the default, but one
    can supply PYSETUP_OPTIONS on the make command-line to override it.

 * The systemd service file read /etc/default/syslog-ng and /etc/sysconfig/syslog-ng,
   but didn't do anything with their contents. $SYSLOGNG_OPTS added to ExecStart, so 
   that the EnvironmentFiles have an effect (at least on Debian).

 * Java support checking fixed (not only jdk is required but also gradle).

 * Memory leak around ping() in Redis fixed.

 * A crash in pdbtool fixed around r_parser_email().

 * Removed cygwin fdlimit statement.
   Make the default for RLIMIT_NOFILE equal to the current system limits.
   --fd-limit can still override this, but the default will be configured 
   based on existing system limits.

 * Fixed BSD year inference.
   Fixed logic and made clearer the inference of year from bsd-style
   rfc3164 syslog-messages, which do not include a year.

 * Handle correctly the epoch 0 timestamp.
   (Previously, syslog-ng cached the zero timestamp and treated 1970 as it was
    1900.)
 
Credits
-------

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Adam Arsenault, Adam Istvan Mozes, Andras Mitzki, Avleen Vig,
Balazs Scheidler, Fabien Wernli, Gergely Czuczy, Gergely Nagy, Gergo Nagy,
Laszlo Budai, Peter Czanik, Robert Fekete, Saurabh Shukla, Tamas Nagy,
Tibor Benke, Viktor Juhasz, Vincent Bernat, Wang Long, Zdenek Styblik,
Zoltan Pallagi.


3.7.1
=====

<!-- Mon, 17 Aug 2015 09:42:17 +0200 -->


New dependencies
----------------

OpenSSL is now a required dependency for syslog-ng because the newly added 
`hostid` and `uniqid` features requires a CPRNG provided by OpenSSL.
Therefore non-embedded crypto lib is not a real option, so the support of 
having such a crypto lib discontinued and all SSL-dependent features enabled
by default.

Library updates
---------------

* Minimal libriemann-client version bumped from 1.0.0 to 1.6.0.
* Added support for the monolithic libsystemd library (systemd 209).
* RabbitMQ submodule upgraded.

Features
--------

### Language bindings

 * Java-destination driver ported from syslog-ng-incubator.
   Purpose of having Java destination driver is to make it possible
   to implement destination drivers in the Java language (and using
   'official' Java client libraries).

 * Python language support is ported from syslog-ng incubator and 
   has been completely reworked. Now, it is possible to implement template
   functions in Python language and also destination drivers.
   Main purpose of supporting Python language is to implement a nice
   interactive syslog-ng config debugger for syslog-ng.

### New drivers

#### New Java destination drivers

ElastiSearch, Kafka and HDFS destination drivers are implemented by using
the 'official' Java client libraries and syslog-ng provides a way to set
their own, native configuration file. Log messages generated by the client 
Java libraries are redirected to syslog-ng via our own Log4JAppender which
means that those logs are available as internal syslog-ng messages.

 * ElasticSearch
 * Kafka
 * Hadoop/HDFS
 * HTTP

### Parsers

* Added a `geoip()` parser, that can look up the country code and
  latitude/longitude information from an IPv4 address. For lat/long to
  work, one will need the City database.

* New parser, `extract-solaris-msgid()` added for automatically extracts
  (parses & removes) the msgid portion of Solaris messages.

* Extended the set of supported characters to every printable ASCII's except
   `.`, `[` and `]` in `extract-prefix` for `json-parser()`.

* Added string-delimiters option to csvparser to support multi character
  delimiters in CSV parsing.

* A kv-parser() introduced for WELF (WebTrens Enhanced Log Format) that 
  implements key=value parsing. The kv-parser() tries to extract 
  key=value formatted name-value pairs from  the input string.


* value-pairs: make it possible to pass --key as a positional argument
  From now it is possible to use value-pairs expressions like this:    
      $(format-json MSG DATE)    
  instead of    
      $(format-json --key MSG --key DATE)


### Filters

* Added IPv6 netmask filter for selecting only messages sent by a host whose
  IP address belongs to the specified IPv6 subnet.

### Macros

 * Added a new macro, called HOSTID which is a 32-bit number generated by
   a cryptographically secure PRNG. Its purpose is to identify the
   syslog-ng host, thus it is the same for every message generated on the same
   host.

 * Added a new macro, called UNIQID which is a practically unique ID generated
   from the `HOSTID` and the `RCPTID` in the format of `HOSTID@RCPTID`. 
   Uniqid is a derived value: it is built up from the always available hostid
   and the optional rcptid. In other words: uniqid is an extension over rcptid.
   For that reason `use-rcptid` has been deprecated and `use-uniqid` could be
   use instead.

### Templates

* welf was renamed to kvformat
  As this reflects the purpose of this module much better, WELF is just
  one of the format it has support for.

* $(format-cim) template function added into an SCL module.
* It is possible to create templates without braces.


### SMTP destination

* The `afsmtp` driver now supports templatable recipients field.
  Just like the subject() and body() fields, now the address containing
  parameters of to(), from(), cc() and bcc() can contain macros.

### Unix Domain Sockets

* Added pass-unix-credentials() global option for enabling/disabling unix 
  credentials passing on those platforms which has this feature. By default
  it is enabled.

* Added create-dirs() option to unix-*() sources for creating the
  containing directories for Unix domain sockets.

### Riemann destination

* Added batched event sending support for riemann destination driver which
  makes the riemann destination respect flush-lines(), and send event
  in batches of configurable amount (defaults to 1). In case of an error,
  all messages within the batch will be dropped. Dropped messages, and
  messages that result in formatting errors do not count towards the batch
  size. There is no timeout, but messages will be flushed upon deinit.

* A timeout() option added to the Riemann destination.

### PatternDB

* Earlier, in patterndb, the first applicable rule won, even if it was
  only a partial match. This means that when rules overlapped, the shorter 
  match would have been found, if it was the first to be loaded.
  A strong preference introduced for rules that match the input string 
  completely. The load order is still applicable though, it is possible to
  create two distinct rules that would match the same input, in those cases
  the first one to be loaded wins.

### Miscellaneous features

* New builtin interactive syslog-ng.conf debugger implemented for syslog-ng.
  The debugger has a Python frontend which contains a full Completer
  (just press TABs and works like bash)

* Added a reset option to syslog-ng-ctl stats. With this option the non-stored
  stats counters can be zeroed.

* New parameter added to loggen: --permanent (-T) wich is for sending logs 
   indefinitely.

* Loggen uses the proper timezone offset in generated message.

* The ssl_options inside tls() extended with the following set:
  no-sslv2, no-sslv3, no-tlsv1, no-tlsv11, no-tlsv12.

* Added syslog-debug bundle generator script to make it easier to reproduce bugs
  by collecting debug related information, like:
    * process information gathering
    * syscall tracing (strace/truss)
    * configuration gathering
    * selinux related information gathering
    * solaris information gathering (sysdef, kstat, showrev, release)
    * get information about syslog-ng svr4 solaris packages, if possible


Bugfixes
--------

* New utf8 string sanitizers instead of old broken one.

* syslog-ng won't send SIGTERM when `getpgid()` fails in program destination
  (`afprog`).

* In some cases program destination respawned during syslog-ng stop/restart
  (`afprog`).

* syslog-ng generates mark messages when `mark-mode` is set
  to `host-idle`.

* Using msg_control only when credential passing is supported in socket 
  destination (`afsocket`).

* Writer is replaced only when protocol changed during reload in socket
  destination (`afsocket`).

* Fix spinning on EOF for `unix-stream()` sockets. Root cause of the spinning
  was that a unix-dgram socket was created even in case of unix-stream.

* When the configured host was not available during the initialization of
  `afsocket` destination syslog-ng just didn't start. From now, syslog-ng
  starts in that case and will retry connecting to the host periodically.

* Fixed BSD year inference in syslogformat. When the difference between the
  current month and the month part of the timestamp of an incoming logmessage
  in BSD format (which has no year part) was greater than 1 then syslog-ng
  computed the year badly.

* In some cases, localtime related macros had a wrong value(eg.:$YEAR).

* TLS support added to Riemann destination

* Excluded "tags" from Riemann destination driver as an attribute which 
  conflicts with reserved keyword

* When a not writeable/non-existent file becomes writeable/exists later,
  syslog-ng recognize it (with the help of reopen-timer) and delivers messages
  to the file without dropping those which were received while the file was
  not available (`affile`).

* Fixed a crash around affile at the first message delivery when templates
  were used (`affile`).

* Fixed a configure error around libsystemd-journal.

* Removed syslog.socket from service file on systems using systemd.
  Syslog-ng reads the messages directly from journal on systems with systemd.

* Fixed compilation where the monolitic libsystemd was not available.

* Fixed compilation failure on OpenBSD.

* AMQP connection process fixed.

* Added DOS/Windows line ending support in config.

* Retries fixed in SQL destination. In some circumstances when
  `retry_sql_inserts` was set to 1, after an insertion failure all incoming
  messages were dropped.

* Transaction handling fixed in SQL destination. In some circumstances when
  both select and insert commands were run within a single transaction and
  the select failed (eg.: in case of mssql), the log messages related to
  the insert commands, broken by the invalid transaction, were lost.

* Fixed a memleak in SQL destination driver.
  The memleak occured during one of the transaction failures.

* Memory leak around reload and internal queueing mechanism has been fixed.

* Fixed a potential abort when the localhost name cannot be detected.

* Security issue fixed around $HOST.
  Tech details:
  When the name of the host is too long, the buffer we use to format the
  chained hostname is truncated. However snprintf() returns the length the
  result would be if no truncation happened, thus we will read uninitialized
  bytes off the stack when we use that pointer to set $HOST
  with log_msg_set_value().

  There can be some security implications, like reading values from the stack
  that can help to craft further exploits, especially in the presense of
  address space randomization. It can also cause a DoS if the hostname length
  is soo large that we would read over the top-of-the-stack, which is probably
  not mmapped causing a SIGSEGV.

* Journal entries containing name-value pairs without '=' caused syslog-ng
  to crash. Instead of crashing, syslog-ng just drop these nv pairs.

* Fixed the encoding of characters below 32 if escaping is enabled in 
  templates. Templated outputs never contained references to characters below
  32, essentially they were dropped from the output for two reasons:

    - the prefixing backslash was removed from the code
    - the format_uint32_padded() function produced no outputs in base 8

* Fixed afstomp destination port issue. It always tried to connect to the port 0.

* Fixed memleak in db-parser which could happen at every reload.

* Fixed a class of rule conflicts in db-parser:
  Because an error in the pdb load algorithms, some rules would conflict which
  shouldn't have done that. The problem was that several programs would use 
  the same RADIX tree to store their patterns. Merging independent programs 
  meant that if they the same pattern listed, it would clash, even though     
  their $PROGRAM is different.

  There were multiple issues:
    * we looked up pattern string directly, even they might have contained
      @parser@ references. It was simply not designed that way and only   
      worked as long as we didn't have the possibility to use parsers     
      in program names

    * we could merge programs with the same prefix, e.g.
      su, supervise/syslog-ng and supervise/logindexd would clash, on "su",
      which is a common prefix for all three.

  The solution involved in using a separate hash table for loading, which
  at the end is turned into the radix tree.

* pdbtool match when used with the --debug-pattern option used a low-level
  lookup function, that didn't perform all the db-parser actions specified
  in the rule

* Max packet length for spoof source is set to 1024 (previously : 256).

* A certificate which is not contained by the list of fingerprints is
  rejected from now.

* Hostname check in tls certificate is case insensitive from now.

* There is a use-case where user wants to ignore an assignment to a name-value
  pair. (eg.: when using `csv-parser()`, sometimes we get a column we really
  want to drop instead of adding it to the message). In previous versions an
  error message was printed out:
  'Name-value pairs cannot have a zero-length name'.
  That error message has been removed.

* Fixed a docbook related compilation error: there was a hardcoded path that
  caused build to fail  if docbook is not on that path. Debian based
  platforms did not affected by this problem.
  Now a new option was created for `./configure` that is `--enable-manpages`
  that enables the generation of manpages using docbook from online source.
  '--with-docbook=PATH' gives you the opportunity to specify the path for
  your own installed docbook.


Credits
-------

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Adam Arsenault, Adam Istvan Mozes, Alex Badics, Andras Mitzki, 
Balazs Scheidler, Bence Tamas Gedai, Ben Kibbey, Botond Borsits, Fabien Wernli,
Gergely Nagy, Gergo Nagy, Gyorgy Pasztor, Kristof Havasi, Laszlo Budai,
Manikandan-Selvaganesh, Michael Sterrett, Peter Czanik, Robert Fekete, 
Sean Hussey, Tibor Benke, Toralf Förster, Viktor Juhasz, Viktor Tusa, 
Vincent Bernat, Zdenek Styblik, Zoltan Fried, Zoltan Pallagi.
