3.31.1
======

## Highlights

 * fortigate-parser(): new parser to parse fortigate logs

   Example:
   ```
   log {
     source { network(transport("udp") flags(no-parse)); };
     parser { fortigate-parser(); };
     destination { };
   };
   ```

   An adapter to automatically recognize fortigate logs in app-parser() has
   also been added.
   ([#3536](https://github.com/syslog-ng/syslog-ng/pull/3536))

 * `patterndb`: Added `OPTIONALSET` parser. It works the same as `SET`, but continues, even if none of the
   characters is found.
   ([#3540](https://github.com/syslog-ng/syslog-ng/pull/3540))

## Features

 * `syslog-parser()`: add no-header flag to tell syslog-ng to parse only the
   PRI field of an incoming message, everything else is just put into $MSG.
   ([#3538](https://github.com/syslog-ng/syslog-ng/pull/3538))
 * `set-pri()`: this new rewrite operation allows you to change the PRI value
   of a message based on the string directly parsed out of a syslog header.
   ([#3546](https://github.com/syslog-ng/syslog-ng/pull/3546))
 * telegram: option to send silent message
   
   Example:
   
   ```
   destination { telegram(bot-id(...) chat-id(...) disable_notification(true)); };
   ```
   ([#3558](https://github.com/syslog-ng/syslog-ng/pull/3558))
 * `app-parser()`: added automatic classification & parsing for project Lumberjack/Mitre CEE formatted logs
   ([#3569](https://github.com/syslog-ng/syslog-ng/pull/3569))
 * diskq: if the dir() path provided by the user does not exists, syslog-ng creates the path with the same permission as the running instance
   ([#3550](https://github.com/syslog-ng/syslog-ng/pull/3550))

## Bugfixes

 * `network()`, `syslog()` destinations: fix reconnection timer when DNS lookups are slow
   
   After a long-lasting DNS query, syslog-ng did not wait the specified time (`time_reopen()`)
   before reconnecting to a destination. This has been fixed.
   ([#3526](https://github.com/syslog-ng/syslog-ng/pull/3526))
 * cmake: minor fixes
   ([#3523](https://github.com/syslog-ng/syslog-ng/pull/3523))
 * `stats-level()`: fix processing the changes in the stats-level() global
   option: changes in stats-level() were not reflected in syslog
   facility/severity related and message tag related counters after first
   configuration reload. These counters continued to operate according to the
   value of stats-level() at the first reload.
   ([#3561](https://github.com/syslog-ng/syslog-ng/pull/3561))
 * `date-parser()`: fix hour-only timezone parsing
   
   If the timestamp contains a short timezone offset (e.g. hours only), the
   recent change in strptime() introduces an error, it interprets those
   numbers as minutes instead of hours. For example: Jan 16 2019 18:23:12 +05
   ([#3555](https://github.com/syslog-ng/syslog-ng/pull/3555))
 * `loggen`: fix undefined timeout while connecting to network sources (`glib < 2.32`)
   
   When compiling syslog-ng with old glib versions (< 2.32), `loggen` could fail due a timeout bug.
   This has been fixed.
   ([#3504](https://github.com/syslog-ng/syslog-ng/pull/3504))
 * `grouping-by()`: fix deadlock when context expires
   
   In certain cases, the `grouping-by()` parser could get stuck when a message
   context expired, causing a deadlock in syslog-ng.
   
   This has been fixed.
   ([#3515](https://github.com/syslog-ng/syslog-ng/pull/3515))
 * `date-parser`: Fixed a crash, which occured sometimes when `%z` was used.
   ([#3553](https://github.com/syslog-ng/syslog-ng/pull/3553))
 * `date-parser`: `%z`. We no longer ignore daylight saving time when calculating the GMT offset.
   ([#3553](https://github.com/syslog-ng/syslog-ng/pull/3553))
 * `kafka-c`: fix a double LogMessage acknowledgement bug, which can cause crash with segmentation fault or exit with sigabrt. The issue affects both flow-controlled and non-flow-controlled log paths and it's triggered in case previously published messages failed to be delivered to Kafka.
   ([#3583](https://github.com/syslog-ng/syslog-ng/pull/3583))
 * `python destination`: Fixed a rare crash during reload.
   ([#3568](https://github.com/syslog-ng/syslog-ng/pull/3568))
 * `date-parser()`: fix non-mandatory parsing of timezone name
   
   When %Z is used, the presence of the timezone qualifier is not mandatory,
   so don't fail that case.
   ([#3555](https://github.com/syslog-ng/syslog-ng/pull/3555))
 * `wildcard-file()`: fix infrequent crash when file renamed/recreated
   
   The wildcard-file source crashed when a file being processed was replaced by
   a new one on the same path (renamed, deleted+recreated, rotated, etc.).
   ([#3513](https://github.com/syslog-ng/syslog-ng/pull/3513))
 * Remove the no-parse flag in system() source from FreeBSD kernel 
   messages, so the message header is no more part of the message.
   ([#3586](https://github.com/syslog-ng/syslog-ng/pull/3586))
 * Fix abort on macOS Big Sur
   
   A basic subset of syslog-ng's functionality now works on the latest macOS version.
   ([#3522](https://github.com/syslog-ng/syslog-ng/pull/3522))
 * `affile`: Fix improper initialization in affile and LogWriter to avoid memory leak when reloading
   ([#3574](https://github.com/syslog-ng/syslog-ng/pull/3574))
 * `udp destination`: Fixed a bug, where the packet's checksum was not calculated,
   when `spoof-source(yes)` and `ip-protocol(6)` were set.
   ([#3528](https://github.com/syslog-ng/syslog-ng/pull/3528))
 * `python`: fix LogMessage.keys() listing non-existenting keys and duplicates
   ([#3557](https://github.com/syslog-ng/syslog-ng/pull/3557))

## Packaging

 * Simplify spec file by removing obsolete technologies:
   - remove RHEL 6 support
   - remove Python 2 support
   - keep Java support, but remove Java-based drivers (HDFS, etc.)
   ([#3587](https://github.com/syslog-ng/syslog-ng/pull/3587))
 * `libnet`: Minimal libnet version is now 1.1.6.
   ([#3528](https://github.com/syslog-ng/syslog-ng/pull/3528))
 * configure: added new --enable-manpages-install option along with the
   existing --enable-manpages. The new option would install pre-existing
   manpages even without the DocBook tools installed.
   ([#3493](https://github.com/syslog-ng/syslog-ng/pull/3493))

## Notes to developers

 * `apphook`: the concept of hook run modes were introduced, adding support for
   two modes: AHM_RUN_ONCE (the original behavior) and AHM_RUN_REPEAT (the new
   behavior with the hook repeatedly called after registration).
   ([#3561](https://github.com/syslog-ng/syslog-ng/pull/3561))

## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

0140454, Andras Mitzki, Antal Nemes, Attila Szakacs, Balazs Scheidler,
egorbeliy, Gabor Nagy, Laszlo Budai, Laszlo Szemere, László Várady,
Michael Ducharme, Norbert Takacs, Peter Czanik, Peter Kokai, Pratik raj,
Ryan Faircloth, Zoltan Pallagi
