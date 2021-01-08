`network()`, `syslog()` destinations: fix reconnection timer when DNS lookups are slow

After a long-lasting DNS query, syslog-ng did not wait the specified time (`time_reopen()`)
before reconnecting to a destination. This has been fixed.
