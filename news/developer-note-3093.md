`example-modules`: add example http slot plugin

This plugin is an example plugin that implements a slot for a HTTP signal (`signal_http_header_request`).

When the plugin is `attached`, it `CONNECT` itself to the signal.
When the signal is emitted by the http module, the slot is executed and append the `header` to the http headers.

`header` is set in the config file.

<details>
  <summary>Example config, click to expand!</summary>

```
version:3.25

@include "scl.conf"

destination d_http {
  http(
    url("http://127.0.0.1:8888")
    method("PUT")
    user_agent("syslog-ng User Agent")
    http-test-slots(
      header("xxx:aaa") # this will be appended to the http headers
    )
    body("${ISODATE} ${MESSAGE}")
  );
};

source s_generator {
  example-msg-generator(num(1) template("test msg\n") );}
;
source s_net {
  network(port(5555));
};

log {
  source(s_generator);
  destination(d_http);
};
```
</details>
