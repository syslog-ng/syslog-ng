`disk-buffer()`: fix crash when switching between disk-based and memory queues

When a disk-buffer was removed from the configuration and the new config was
applied by reloading syslog-ng, a crash occurred.
