`logwriter`: Fixed a reconnect hang that could occur when a reopen was deferred while the writer's I/O job was
still in flight, leaving the writer with no proto and nothing to wake it up again. Also fixed a related issue
where a stale reopen timer could outlive a subsequently installed valid connection and trigger a spurious,
duplicate reconnect.
