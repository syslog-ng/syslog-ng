Fix threaded destination crash during a configuration revert

Threaded destinations that do not support the `workers()` option crashed while
syslog-ng was trying to revert to an old configuration.
