`cfg`: Fixed `@include` on Windows: relative includes along the default include path (e.g. `scl.conf`) and absolute
paths with a drive letter were not found, and config error locations inside SCL blocks lost the backslashes of
file names. Also fixed installing modules and listing them in `syslog-ng --version` in native Windows builds.
