afsocket: syslog-ng fails to bind() after config revert

When having a program source or destination and a network destination in the
config, if we reload with an invalid config, syslog-ng crashes, as it cannot init
the old network source, because its address is in use.
