`network-dest`, `syslog-dest`: Fixed switching a UDP destination between `network()` and `syslog()` not taking
effect on reload. Previously the old message format was kept until syslog-ng was fully restarted.
