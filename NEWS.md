3.7.0alpha2
===========

<!-- Mon, 15 Dec 2014 14:49:33 +0100 -->

This is the second alpha release of the syslog-ng OSE 3.7
branch.

Changes compared to the previous alpha release:

Features
--------

 * Added support for the monolithic libsystemd library (systemd 209).

 * New parameter added to loggen: --permanent (-T) wich is for sending logs 
   indefinitely.

 * Earlier, in patterndb, the first applicable rule won, even if it was
   only a partial match. This means that when rules overlapped, the shorter 
   match would have been found, if it was the first to be loaded.
   A strong preference introduced for rules that match the input string 
   completely. The load order is still applicable though, it is possible to
   create two distinct rules that would match the same input, in those cases
   the first one to be loaded wins.

 * New parser, `extract-solaris-msgid()` added for automatically extracts
   (parses & removes) the msgid portion of Solaris messages.

Fixes
-----

 * In some cases program destination respawned during syslog-ng stop/restart.

 * Max packet length for spoof source is set to 1024 (previously : 256).

 * Removed syslog.socket from service file on systems using systemd.
   Syslog-ng reads the messages directly from journal on systems with systemd.

 * In some cases, localtime related macros had a wrong value(eg.:$YEAR).

 * Transaction handling fixed in SQL destination. In some circumstances when
   both select and insert commands were run within a single transaction and
   the select failed (eg.: in case of mssql), the log messages related to
   the insert commands, broken by the invalid transaction, were lost.

 * Fixed a memleak in SQL destination driver.
   The memleak occured during one of the transaction failures.

 * A certificate which is not contained by the list of fingerprints is
   rejected from now.

 * Hostname check in tls certificate is case insensitive from now.

 * Fix spinning on EOF for `unix-stream()` sockets. Root cause of the spinning
   was that a unix-dgram socket was created even in case of unix-stream.

 * There is a use-case where user wants to ignore an assignment to a name-value
   pair. (eg.: when using `csv-parser()`, sometimes we get a column we really
   want to drop instead of adding it to the message). In previous versions an
   error message was printed out:
   'Name-value pairs cannot have a zero-length name'.
   That error message has been removed.

 * pdbtool match when used with the --debug-pattern option used a low-level
   lookup function, that didn't perform all the db-parser actions specified
   in the rule

Developer notes
---------------

 * PatternDB lookup refactored (it is easier to understand the code).

Credits
-------

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Andras Mitzki, Balazs Scheidler, Fabien Wernli, Gergely Nagy, Laszlo Budai,
Michael Sterrett, Peter Czanik, Robert Fekete, Tibor Benke, Sean Hussey, 
Viktor Juhasz, Viktor Tusa, Zoltan Fried .

3.7.0alpha1
===========

<!-- Fri, 07 Nov 2014 12:39:45 +0100 -->

This is the first alpha release of the syslog-ng OSE 3.7
branch.

Changes compared to the latest stable release (3.6.1):

Features
--------

 * It is possible to create templates without braces.

 * User defined template-function support added.
   User can define template functions in her/his configuration the same
   way she/he would define a template.

 * $(format-cim) template function added into an SCL module.

 * A new choice for inherit-properties implemented that will merge
   all name-value pairs into the new synthetic message, with the most recent
   being beferred over older values.

Developer notes
---------------

 * Added implementation for user-defined template functions.
   A new API added, user_template_function_register() that allows
   registering a LogTemplate instance as a template function, dynamically.

Credits
-------

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Andras Mitzki, Balazs Scheidler, Fabien Wernli, Gergely Nagy, Laszlo Budai,
Peter Czanik, Viktor Juhasz, Viktor Tusa
