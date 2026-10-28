`network`/`syslog` sources: fix `dynamic-window-size()` handling across reloads

Fixed `dynamic-window-size()` being lost after a reload that changed `keep-alive(yes)` to `keep-alive(no)`
with `so-reuseport(yes)`; new connections only got the static window until the next reload. Fixed a crash
(SIGSEGV in the dynamic window reallocation) after a reload that turned `dynamic-window-size()` on while
connections were kept, or that changed `keep-alive(yes)` to `keep-alive(no)` with `so-reuseport(yes)`.
Connections that the new configuration does not keep are now closed when the reload completes, instead of
staying open but unread until the next reload.
