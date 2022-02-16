`system()` source: added basic support for reading macOS system logs

The current implementation processes the output of the original macOS syslogd:
`/var/log/system.log`.
