`loggen`: fix undefined timeout while connecting to network sources (`glib < 2.32`)

When compiling syslog-ng with old glib versions (< 2.32), `loggen` could fail due a timeout bug.
This has been fixed.
