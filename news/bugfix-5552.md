`afsocket-source`: Fixed two `keep-alive()` config reload–related issues.
First, when a reload switched from `keep-alive(yes)` to `keep-alive(no)` with `so-reuseport(no)`, the newly loaded config’s socket instance could fail to open.
Second, in case of an error in the new config, the reload could fail to properly revert the connection state.
