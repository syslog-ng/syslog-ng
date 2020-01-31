`azure-auth-header`: Azure auth header is a plugin that generates
authorization header for applications connecting to Azure.
It can be used as a building block in higher level SCLs.
Implemented as a `signal-slot` plugin.

<details>
  <summary>Example config, click to expand!</summary>

```
@version:3.25
@include "scl.conf"
destination d_http {
  http(
    url("http://127.0.0.1:8888")
    method("PUT")
    user_agent("syslog-ng User Agent")
    body("${ISODATE} ${MESSAGE}")
    azure-auth-header(
      workspace-id("workspace-id")
      secret("aa1a")
      method("POST")
      path("/api/logs")
      content-type("application/json")
    )
  );
};
source s_gen {
  example-msg-generator(num(1) template("Test message\n"));
};
log {
  source(s_gen);
  destination(d_http);
};
```
</details>
