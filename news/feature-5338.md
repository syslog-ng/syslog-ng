`file()`, `wildcard-file()`: Added `follow-method()` option.

|Accepted values:| legacy \| inotify \| poll \| system |

This option controls how syslog-ng will follow file changes.\
The default `legacy` mode preserves the pre-4.9 version file follow-mode behavior of syslog-ng, which is based on the value of follow-freq().\
The `poll` value forces syslog-ng to poll for file changes at the interval specified by the monitor-freq() option, even if a more efficient method (such as `inotify` or `kqueue`) is available.\
If `inotify` is selected and supported by the platform, syslog-ng uses it to detect changes in source files. This is the most efficient and least resource-consuming option available on Linux for regular files.\
The `system` value will use system poll methods (via ivykis) like `port-timer` `port` `dev_poll` `epoll-timerfd` `epoll` `kqueue` `ppoll` `poll` and `uring`. For more information about how to control the system polling methods used, see [How content changes are followed in file() and wildcard-file() sources](https://syslog-ng.github.io/admin-guide/060_Sources/020_File/001_File_following).
