`cfg`: allow usage of `current` in config @version by default if it is not presented

This change allows syslog-ng to start even if the `@version` information is not present in the configuration file and treats the version as the latest in that case.

**NOTE:** syslog-ng will still raise a warning if `@version` is not present. Please use `@version: current` to confirm the intention of always using the latest version and silence the warning.
