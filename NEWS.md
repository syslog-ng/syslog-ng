3.28.1
======

## Highlights

 * `http`: add support for proxy option

   Example:
   ```
   log {
      source { system(); };
      destination { http( url("SYSLOG_SERVER_IP:PORT") proxy("PROXY_IP:PORT") method("POST") ); };
   };
   ```
   ([#3253](https://github.com/syslog-ng/syslog-ng/pull/3253))

## Features

 * `map`: template function

   This template function applies a function to all elements of a list. For example: `$(map $(+ 1 $_) 0,1,2)` => 1,2,3.
   ([#3301](https://github.com/syslog-ng/syslog-ng/pull/3301))
 * `use-syslogng-pid()`: new option to all sources

   If set to `yes`, `syslog-ng` overwrites the message's `${PID}` macro to its own PID.
   ([#3323](https://github.com/syslog-ng/syslog-ng/pull/3323))

## Bugfixes

 * `affile`: eliminate infinite loop in case of a spurious file path

   If the template evaluation of a log message will result to a spurious
   path in the file destination, syslog-ng refuses to create that file.
   However the problematic log message was left in the msg queue, so
   syslog-ng was trying to create that file again in time-reopen periods.
   From now on syslog-ng will handle "permanent" file errors, and drop
   the relevant msg.
   ([#3230](https://github.com/syslog-ng/syslog-ng/pull/3230))
 * Fix minor memory leaks in error scenarios
   ([#3265](https://github.com/syslog-ng/syslog-ng/pull/3265))
 * `crypto`: fix hang on boot due to lack of entropy
   ([#3271](https://github.com/syslog-ng/syslog-ng/pull/3271))
 * Fix IPv4 UDP destinations on FreeBSD

   UDP-based destinations crashed when receiving the first message on FreeBSD due
   to a bug in destination IP extraction logic.
   ([#3278](https://github.com/syslog-ng/syslog-ng/pull/3278))
 * `network sources`: fix TLS connection closure

   RFC 5425 specifies that once the transport receiver gets `close_notify` from the
   transport sender, it MUST reply with a `close_notify`.

   The `close_notify` alert is now sent back correctly in case of TLS network sources.
   ([#2811](https://github.com/syslog-ng/syslog-ng/pull/2811))
 * `disk-buffer`: fixes possible crash, or fetching wrong value for logmsg nvpair
   ([#3281](https://github.com/syslog-ng/syslog-ng/pull/3281))
 * `packaging/debian`: fix mod-rdkafka Debian packaging
   ([#3282](https://github.com/syslog-ng/syslog-ng/pull/3282))
 * `kafka destination`: destination halts if consumer is down, and kafka's queue is filled
   ([#3305](https://github.com/syslog-ng/syslog-ng/pull/3305))
 * `file-source`: Throw error, when `follow-freq()` is set with a negative float number.
   ([#3306](https://github.com/syslog-ng/syslog-ng/pull/3306))
 * `stats-freq`: with high stats-freq syslog-ng emits stats immediately causing high memory and CPU usage
   ([#3320](https://github.com/syslog-ng/syslog-ng/pull/3320))
 * `secure-logging`: bug fixes ([#3284](https://github.com/syslog-ng/syslog-ng/pull/3284))
    - template arguments are now consistently checked
    - fixed errors when mac file not provided
    - fixed abort when derived key not provided
    - fixed crash with slogkey missing parameters
    - fixed secure-logging on 32-bit architectures
    - fixed CMake build

## Other changes

 * `dbld`: Fedora 32 support ([#3315](https://github.com/syslog-ng/syslog-ng/pull/3315))
 * `dbld`: Removed Ubuntu Eoan ([#3313](https://github.com/syslog-ng/syslog-ng/pull/3313))
 * `secure-logging`: improvements ([#3284](https://github.com/syslog-ng/syslog-ng/pull/3284))
    - removed 1500 message length limitation
    - `slogimport` has been renamed to `slogencrypt`
    - `$(slog)` will not start anymore when key is not found
    - internal messaging (warning, debug) improvements
    - improved memory handling and error information display
    - CMake build improvements
    - switched to GLib command line argument parsing
    - the output of `slogkey -s` is now parsable
    - manpage improvements

## Notes to developers

 * `dbld`: devshell is now upgraded to Ubuntu Focal
   ([#3277](https://github.com/syslog-ng/syslog-ng/pull/3277))
 * `dbld/devshell`: Multiple changes:
    * Added snmptrapd package.
    * Added support for both `python2` and `python3`.
   ([#3222](https://github.com/syslog-ng/syslog-ng/pull/3222))
 * `threaded-source`: fully support default-priority() and default-facility()
   ([#3304](https://github.com/syslog-ng/syslog-ng/pull/3304))
 * `CMake`: fix libcap detection
   ([#3294](https://github.com/syslog-ng/syslog-ng/pull/3294))
 * Fix atomic_gssize_set() warning with new glib versions
   ([#3286](https://github.com/syslog-ng/syslog-ng/pull/3286))

## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Airbus Commercial Aircraft, Andras Mitzki, Antal Nemes, Attila Szakacs,
Balazs Scheidler, Gabor Nagy, Laszlo Budai, Laszlo Szemere, László Várady,
Péter Kókai, Vatsal Sisodiya, Vivin Peris.
