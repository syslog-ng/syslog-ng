`syslog-parser()`: allow comma (e.g. ',') to separate the seconds and the fraction of a
second part as some devices use that character. This change applies to both
to syslog-parser() and the builtin syslog parsing functionality of network
source drivers (e.g. udp(), tcp(), network() and syslog()).
