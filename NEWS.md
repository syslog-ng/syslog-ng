3.8.1
=====

<!-- Fri, 19 Aug 2016 10:07:25 +0200 -->


# Library updates
  * Kafka-client updated to version to 0.9.0.0
  * Minimal required version of hiredis is set to 0.11.0 to avoid
    possible deadlocks
  * Minimal version of libdbi is set to 0.9.0

# New dependencies
 * From now `autoconf-archive` package is a build-dependency.

# Improvements and features
  * Added the long-waited disk-buffer.
  * `date-parser` ported from incubator to upstream
  * New template functions: min, max, sum, average
  * Added Apache-accesslog-parser
  * Added loggly destination
  * Added logmatic destination
  * Added template function for supporting CEF.
  * cURL-based HTTP destination driver added (implemented in C 
    programming language)
  * SELinux policy installer script now has support for Red Hat Enterprise Linux/CentOS/
     Oracle Linux 5, 6 and 7.
  * Implemented `add-contextual-data`:
    With `add-context-data` syslog-ng can use an external database file to append
    custom name-value pairs on incoming logs (to enrich messages). The 'database'
    is actually a file that containing `<selector, name, value>` records.
    Currently only `CSV` format is supported.    
    It is like `geoip parser` where the selector is `$HOST`, but in this case,
    the user can define the selector, and also the database contents.

## Drivers
  * Program destination/source drivers
    * Added inherit-environment configuration option to program source and
      destination. When it is set to true then the process will inherit the
      entire environment of the parent process.
    * Added `keep-alive` option to program destination (afprog).
      This option will control whether the destination program should be
      terminated at reload, or should be left running.

  * Java drivers
    * HTTP destination
      * Added the ability to use templates in both url and message.
    * ElasticSearch Destination driver :
      * Support 2.2.x series of ElasticSearch (transport and node mode) .
      * Support Shield plugin for both ElasticSearch 1.x and ElasticSearch
        2.x .
      * Implemented new mode (HTTP) that can work with ElasticSearch 1.x,
        ElasticSearch 2.x, and even with Elastic 5. HTTP mode is based on
        a Java HTTP Rest client (Jest : https://github.com/searchbox-io/Jest).
        Note: `make install` will copy Jest library to the syslog-ng install
        directory.

  * MongoDB destination driver
    * Replaced submodule limongo-client with mongo-c-driver.
    * Additional support for previous syntax used by libmongo-client
    before we started using mongo-c-driver and its URI syntax exclusively.
    Note that these are plainly translated to a connection URI without
    much sanity checking or preserving their former semantic meaning.
    So various aspects of the MongoDB connection like health checks, retries,
    error reporting and synchronicity will still follow the slightly altered
    semantics of mongo-c-driver.

  * Riemann destination driver
    * Use cert-file() and key-file() options to match afsocket keywords as
      the same way as afsocket drivers use these options. The old one still
      work though.

## Rewrite rules
  * Introduced template options in rewrite rules.
  * Added unset operation to make it possible to unset a specific name-value pair
    for a logmessage.

## Parsers
  * kvformat: make it possible to specify name-value separator
  * linux-audit-scanner: recognize a0-a9* as fields to be decoded
    Argument lists are encoded in a0, a1, ... fields that can potentially be
    hex-encoded.
  * `csv-parser` has been refactored, extended with new dialect and prefix options.
    Dialect is to convey CSV format information, instead of using flags
    Prefix option gets prefixed to all column names, just like with
    other parsers.

## PatternDB
  * added groupingby() parser that can perform simple correlation on
    log messages. In a way it is similar to the SQL GROUP BY operation, where
    an aggregate of a set of input records can be calculated.
    The major difference between SQL GROUP BY and groupingby() is that the
    first _always_ operates on a enumerable list of records, whereas
    groupingby() works on a stream of data. 
    A few use-cases where this can be useful:
      * Linux audit logs
      * postfix logs

  * added create-context action    
    Added a new possible action in the <actions> element, to create
    a new correlation context out of the current message and its associated
    context. This can be used to "split" a state.

  * Added NLSTRING parser that captures a string until the
    following end-of-line. It can be used in patterns as: @NLSTRING:value@
    It doesn't expect any additional parameters. This makes it pretty easy to
    parse multi-line Windows logs.

## Miscellaneous features
 * syslog-debun (debug bundle script for syslog-ng) has been improved

# Bugfixes
 * geoip-parser: When default database if not specified, syslog-ng crashed.
 * Added support for multiple drivers with the same name in syslog-ng config.
 * Fixed aack counting logic for junctions that have branches that modify
   the LogMessage.
 * Fixed a potential crash for code that uses log_msg_clear()
   in production (e.g. syslog-parser()).
 * Fixed potential crash in reload logic
 * system(): use string comparison instead of numeric in PID rewrite
   The meaning of the != operator has been fixed to refer to numeric comparison
   in @version: 3.8, so make sure we are using string comparison.
 * Support encoding on glib compiled with libiconv
 * pdbtool: Fix the ordering of the debug-info list in PatternDB 
 * afprog: Don't kill our own process group    
    If, for some reason, the program source/destination failed to set up its
    own process group, we need to make sure we do not run killpg() on that
    process group, as it would kill ourselves.
 * Handle option names with hyphen (-) characters in java scls  
 * dnscache performance improved
    Instead of getting rid off the per-thread DNSCache when a worker thread
    exits, store them in a linked list and acquire them as a new thread starts.
    The set of cached hostnames are valuable as worker threads come and
    go (they exit after 10seconds of inactivity), but without this
    reusing of cache instances, our DNS cache is filled again and again.
 * Fixed IPv6 parser in patterndb.
 * Fixed journald program name flapping
 * Fixed create-dirs() inheritance in file destinations
 * Fixed pass-unix-credentials() global inheritance in afunix
    The global `pass-unix-credentials` option was not inherited in afunix-source
    if the `options{};` block was positioned lower in the
    configuration file than the given module declaration.
 * Fixed create-dirs() global inheritance in afunix
   When the global `create-dirs` option was set to `yes`, the local one was ignored.
 * Fixed byteorder handling on bigendian systems in netmask6 filter
 * Fixed flow-control issue when overflow queue is full
   (suspending source by setting the window size to 0).
 * Log HTTP response error codes in HTTPDestination (Java).
 * Fixed potential leaks related $(sanitize) argument parsing in basicfuncs.
 * Fixed a memory leak in python debugger
 * Fixed a use-after-free bug in templates.
 * Fixed a memory leak around reload in netmask6 filter.
 * Fixed a memory leak in LogProtoBufferedServer in case the
   encoding() option is used.
 * configure: don't override $enable_python while executing pkg-config
 * Fixed BSD timestamp parsing in syslog-format.
 * Fixed a SIGPIPE bug in program destination.   
 * Error handling has been improved in AMQP destination.
 * value-pairs performance improvements, memleak fixes
 * Various issues around UTF-8 support fixed.
 * Fixed integer overflow in numerical operations template function
 * Fixed an integer underflow in afsocket.
 * Fixed numerical comperisons issues around filters.
    There's a problem in straight fixing this issue though: anyone who used
    the numeric operators erroneously will have their behaviour changed, therefore
    this patch also adds a configuration update warning in case
    someone is using the wrong syntax.
 * Fixed kernel log message time drift on Linux.
 * Take CRLF sequences equivalent to an LF in patterndb.    
    Windows logs contain embedded CRLFs which is difficult to match against
    from db-parser(), as we use a UNIX text file to store the patterns. Also,
    the fact that the input contains CRLF whereas our patterns only contain
    an LF makes it a very unintuitive non-match, which is difficult to debug.
 * When syslog-ng failed to insert data into Redis, it has crashed.
 * When device file is set as a file destination then syslog-ng will not try
   to change the permission of the device file.
 * Various fixes around config file parsing:
    * in some circumstances syslog-ng crashed when the config
      file contained non-readable characters
    * fixed a memory leak
    * fixed memory leak around backtick substitution

# Notes to the Developers
 * copyright cleanup in source tree

 * install tools and scl under a syslog-ng specific subdirectory    
   These should never be installed in /usr/share directly, but rather under a
    subdirectory and as described in    
    https://www.gnu.org/prep/standards/html_node/Directory-Variables.html
    we should do that right within the source and not rely on packaging tools
    to do it for us.This will trigger a required change in packaging scripts to
    avoid changing the --datadir, as the default of
    /usr/share should work out-of-the-box.

 * Support for native-lanugage (compiled languages, like Rust) bindings.
    These bindings just forward the calls to the native side.    
    This whole module compiles into a static library
    (libsyslog-ng-native-connector.a) which is linked to all external native
    modules. A native module defines the required functions
    (like native_parser_proxy_new()) so those symbols will be resolved.    
    Some symbols have the visibility(hidden) attribute applied to them. Those
    symbols are defined by the other half of the native bindings, we only need
    their signature here. They are hidden because their definition is contained
    in other source files but we would like to keep them as "library private".
    If they are exported, the dynamic linker will resolve them when a module is
    loaded, therefore every module would be mapped to the first loaded one.
    It is best to hide everything in this native connector.
    

 * Support system librabbitmq-c (AMQP destination)    
    Currently only the internal librabbitmq-c is supported,
    if we want to use the preexisting library, the configuration
    will fail.
    This change is required if we want to get rid of the
    internal libraries.
    
 * Added `check-valgrind` target to Makefile

 * Remove dependency on Gradle for building Java language bindings
  (not modules, just the language binding)

 * Experimental CMake support added
 * Experimental OSX support added
 * Improved Travis build matrix
 * Added plugin skeleton creator.
 * Debian packaging improved: Debian packaging from unofficial 
   OBS repository has been ported to upstream.
   From now, someone could easily build debian packages from upstream source.

#Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Adam Istvan Mozes, Andras Mitzki, Arnaud Vamorec, Balazs Scheidler,
David Schweikert, Fabien Wernli, Flavio Medeiros, Hanno Böck,
Henrik Grindal Bakken, Gergo Nagy, Gyorgy Pasztor, Laszlo Budai,
Laszlo Varady, Marc Falzon, Noemi Vanyi, Peter Czanik, Robert Fekete,
Tibor Benke, Viktor Juhasz, Viktor Tusa, Vincent Bernat, Zdenek Styblik,
Zoltan Fried, Zoltan Pallagi, Yilin Li
