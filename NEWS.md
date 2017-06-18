3.10.1

<!-- Sun, 18 Jun 2017 16:26:18 +0200 -->

# Features

 * Support https in http (curl) module

 * Docker support : from now Dockerfile for CentOS7, Ubuntu Zesty and for
   Debian Jessie is part of our upstream

 * Add --database parameter for geoip template function

 * Metric improvements
     * add discarded messages for parsers
     * add matched/not matched counter for filters
     * add memory_usage counter to logqueue
     * add written counter
        * Written is a calculated counter which return the written messages
          by destinations. Written message is which was processed but not
          queued and not dropped. (written = processed - queued - dropped)
     * stats-counters: rename stored counter to queued
     * add global_allocated_logmsg_size counter for tracking memory logmsg
       related allocations

 * Add snmp-parser (v1, v2)
     * parses snmptrapd log
     * The parsed information is available as key-value pairs, which can be
       used/serialized (macros, format-json, etc.) in the log path.
       If you want to send the message in a structured way, you can disable the
       default message generation with the `generate-message(no)` option.

 * Add snmp-soure
    * available as an SCL block that containing a filesource and an SNMP parser
    modules: add snmptrapd parser

 * Add osquery source
   * available as an SCL block
   * It reads the osquery log file and parses with the JSON parser,
     creating name-vaule pairs with an .osquery. prefix by default.

 * Add cisco-parser
   * available as an SCL block

 * Add wildcard filesource

 * Add startdate template function

 * Add $(basename) and $(dirname) template functions

 * Add Kerberos support for HDFS destination

 * Add AUTH support for redis destination

 * Add map-value-pairs() parser
    * it can be used to map existing name-value pairs to a different set during
      processing, in bulk.  Normal value-pairs expressions can be used, just
      like with value-pairs based destinations.

 * Extend Python language binding by Python parser

 * Add support for extract-stray-words() option in kv-parser()
    * stray words: those words that happen to be between key-value pairs and
      are otherwise not recognized either as keys nor as values.

 * Add $(context-values) template function

 * Add $(context-lookup) function

 * Add list related template functions
    * $(list-head list ...)          returns the first element (unquoted)
    * $(list-nth NDX list ...)       returns the specific element (unquoted)
    * $(list-concat list1 list2 ...) returns a list containing the concatenated
                                     list
    * $(list-append list elem1 elem2 ...) returns a list, appending elem1,
                                     elem2 ...
    * $(list-tail list ...)          returns a list containing everything except
                                     for the first element
    * $(list-slice FROM:TO list ...) returns a list containing the slice
                                     [FROM:TO), Python style slice
                                   boundaries are supported (e.g. negative)
    * $(list-count ...)              returns the number of elements in list

 * Add add query commands to syslog-ng-ctl
    * query list	<filter>		 List names of counters which match the filter
    * query get <filter>			 Get names and values of counters which match the
                               filter
    * query get --sum <filter> Get the sum of values of counters which match the
                               filter

 * Support multiple servers in elasticsearch2-http destination

 * Implements elastic-v2 https in http mode

 * Add getent module (ported from incubator)
    * This module adds $(getent) that allows one to look up various NSS based
      databases, such as passwd, services or protocols.

 *  Add support for IP_FREEBIND


# Bugfixes

 * Fix a libnet detection check error that caused problem configuring
   enable-spoof-source.

 * Avoid warnings about _DEFAULT_SOURCE on recent glibc versions
   With the glibc on zesty, using _GNU_SOURCE and not defining _DEFAULT_SOURCE
   results in a warning, avoid that by defining _DEFAULT_SOURCE as well.

 * Fix invalid database warning for geoip parser

 * Fix prefix() default in systemd-journal for new config versions

 * Fix a potential message loss in Riemann destination

 * Fix a potential crash in the Riemann destination when the client is not
   connected to the Riemann server.

 * Fix a possible add-contextual-data() related data loss in case of multiple
   reference to the same add-contextual-data parser in several logpaths.

 * Fix dbparser deadlock

 * Fix Python destination
     * open() was not called in every time_reopen()
     * python destination is not defined in stats output

 * Fix processed stats counter for afsocket

 * Fix stats source for pipes
   * Previously pipe source is shown as file

 * Fix csv-parser multithreaded support
   In some cases (when csv-parser attached to network source), the parser
   randomly filled the column macros with garbage.

 * Fix a message loss in case of filesource when syslog-ng was restarted and
   the log_msg_size > file size.

 * Fix a potential crash in cryptofuncs

 * Fix a potential crash in syslog-ng-ctl when no command line parameters was
   set.

 * Fix token duplication in the output of '--preprocess-into'

 * Fix UTF-8 support in syslog-ng-ctl

 * Fix a potential crash during X.509 certificate validation.

 * Fix a segfault in Python module startup

 * Fix a possible endless reading loop issue in case of multi-line filesource.

 * Fix soname for the http module from "curl" to "http"

 * Avoid openssl 1.1.0 deprecated APIs
   When openssl is built with `--api=1.1 disable-deprecated`, use of deprecated
   APIs results in build failure.



# Other changes

 * Increase processed counter by queued counter after reload or restart when
   diskqueue is used otherwise the newly added written counter would underflow.

 * Set the default time-zone to UTC for elasticsearch2
   Elasticsearch and Kibana use UTC internally.

 * Add retries support for python destination

 * Prefer server side cipher suite order

 * Always include librabbitmq in the dist tarball

 * Always include ivykis in the dist tarball

 * Marking parse error locations with >@<.

 * Default log_msg_size is increased to 64Kbyte from 8Kb
 
 * Tons of syslog-debun improvements

 * Exit with 0 return code when --help is specified for syslog-ng-ctl

 * syslog-ng: make '--preprocess-into' foreground only

 * Add debug messages on log_msg_set_value()

 * Add more detail to filter evaluation related debug messages


# Notes to the Developers

 * Extract template perf test function to testlib

 * Print a debug message when logmsg passed to the Python side

 * Allow http module (curl) to be build with cmake

 * astylerc: allow continuation lines to start until column 60

 * Move kv-scanner under syslog-ng/lib

 * scratch-buffers2: implement an alternative to current scratch buffers
    This new API is aimed a bit easier to use in situations where a throw away
    buffer is needed that will automatically be freed at the next message.
    It also gets does away with GTrashStack that is deprecated in recent glib
    versions.

 * Several refactors in stats module.

# Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Andras Mitzki, Antal Nemes, Balazs Scheidler, eroen, Fabien Wernli, Gabor Nagy,
Gergely Nagy, Jason Hensley, Laszlo Varady, Laszlo Budai, Mate Farkas, 
Noemi Vanyi, Peter Czanik, Peter Gervai, Todd C. Miller, Philip Prindeville,
Zoltan Pallagi


