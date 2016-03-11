http destination
================

The http destination can send the log as POST HTTP requests to an HTTP
server. It supports setting url, headers, user\_agent, authentication 
and body.

Example config:

```
@version: 3.7
@include "scl.conf"
source      s_system { system(); internal(); };
destination http_des {
    http(
        url("http://127.0.0.1:8000")
        user_agent("syslog-ng User Agent")
        user("user")
        password("password")
        headers("HEADER1: header1", "HEADER2: header2")
        body("BODY")
    );
};
log { source(s_system); destination(http_des); };
```
