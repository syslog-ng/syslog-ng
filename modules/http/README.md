http destination
================

The http destination can send the log as HTTP requests to an HTTP server.
It supports setting url, method, headers, user\_agent, authentication
and body. Only PUT and POST method is supported so far. If the method is
not set, POST will be used.

Example config:

```
@version: 3.25
@include "scl.conf"
source      s_system { system(); internal(); };
destination http_des {
    http(
        url("http://127.0.0.1:8000")
        method("PUT")
        user_agent("syslog-ng User Agent")
        user("user")
        password("password")
        headers("HEADER1: header1", "HEADER2: header2")
        body("${ISODATE} ${MSG}")
    );
};
log { source(s_system); destination(http_des); };
```
