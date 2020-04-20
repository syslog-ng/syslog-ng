`mongodb`: Replaced the deprecated `get_server_status()` API with `command_simple()`.
This means, that `syslog-ng` can now be built with `-Werror` on systems with 1.15 `libmongoc`.
