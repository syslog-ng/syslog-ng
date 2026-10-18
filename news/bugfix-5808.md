`network`/`syslog` sources: Fixed `dynamic-window-size()` being lost after a reload that changed
`keep-alive(yes)` to `keep-alive(no)` with `so-reuseport(yes)`; new connections only got the static window
until the next reload.
