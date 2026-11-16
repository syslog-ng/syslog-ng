http destination
================

The http destination can send the log as HTTP requests to an HTTP server.
It supports setting url, method, headers, user\_agent, authentication
and body. Only PUT and POST method is supported so far. If the method is
not set, POST will be used.

Example config:

```
@version: 3.28
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

### response-body-action()

Some HTTP APIs answer `200` and report the failure in the body. `response-body-action()` takes
`"regexp" => success|retry|drop|disconnect` pairs, matched in order against the body of a response
that the status code mapped to success; the first match decides the result.

```
http(
    url("http://doris:8040/api/db/table/_stream_load")
    response-body-action(
        "DATA_QUALITY_ERROR" => drop
        "(?i)\"status\":\\s*\"fail\"" => retry
    )
);
```

* The patterns are PCRE2 regular expressions compiled with default options, so matching is
  case-sensitive. Use the inline modifier `(?i)` at the start of a pattern for case-insensitive
  matching.
* Only the first 64 KiB of the body (`HTTP_RESPONSE_BODY_MATCH_MAX_LENGTH`) are captured for
  matching, and only when `response-body-action()` is configured. A longer body is truncated, and a
  marker after the limit is not seen; the debug log says so ("response body was truncated").
  Responses are logged up to 1 KiB regardless.
