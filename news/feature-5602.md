`file()`, `wildcard-file()` source: Add `auto` follow-method() option

A new follow-method() option, auto, has been added. In this automatic mode, syslog-ng OSE uses the following sequence to decide which method to use:

- the `inotify` method is used automatically if the system supports it (and none of the IV_SELECT_POLL_METHOD or IV_EXCLUDE_POLL_METHOD environment variables are set); otherwise
- the best available (or the IV_SELECT_POLL_METHOD or IV_EXCLUDE_POLL_METHOD forced) `system` (ivykis) poll method of the platform is used; if none is available,
- the old `poll` method is used

Unfortunately, for backward compatibility reasons, the new `auto` option could not become the default at this time, but for new configurations, we recommend using monitor-method("auto") and follow-method("auto") to achieve the best available performance on the given platform.
