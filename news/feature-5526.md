mod-python: replace `Logger` class with internal log handler

Previous `Logger` python class is replaced with internal messages handler, which is installed
into logging pipeline, allowing python modules to log messages via default logging facilities.
Also, `TRACE` log level is now explicitly exported and could be used with `log()` methods or `trace()`
call directly.
