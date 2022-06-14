`pdbtool`: fix a SIGABRT on FreeBSD that was triggered right before pdbtool
exits. Apart from being an ugly crash that produces a core file,
functionally the tool behaved correctly and this case does not affect
syslog-ng itself.
