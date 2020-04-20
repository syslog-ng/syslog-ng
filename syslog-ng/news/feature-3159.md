http: When a HTTP response is received, emit a signal with the HTTP response code.
(Later it can be extended to read the response and parse it in a slot...).

This PR also extends the Python HTTP header module with the possibility of writing custom HTTP response code handlers. When someone implements an auth header plugin in Python, it could be useful (for example invalidating a cache).

<details>
  <summary>Example config, click to expand!</summary>

```

@version: 3.25

python {
from syslogng import Logger

logger = Logger()

class TestCounter():
    def __init__(self, options):
        self.header = options["header"]
        self.counter = int(options["counter"])
        logger.debug(f"TestCounter class instantiated; options={options}")

    def get_headers(self, body, headers):
        logger.debug(f"get_headers() called, received body={body}, headers={headers}")
       
        response = ["{}: {}".format(self.header, self.counter)]
        self.counter += 1
        return response

    def on_http_response_received(self, http_code):
        self.counter += http_code
        logger.debug("HTTP response code received: {}".format(http_code))

    def __del__(self):
        logger.debug("Deleting TestCounter class instance")
};

source s_network {
  network(port(5555));
};

destination d_http {
    http(
        python_http_header(
            class("TestCounter")
            options("header", "X-Test-Python-Counter")
            options("counter", 11)
            # this means that syslog-ng will trying to send the http request even when this module fails
            mark-errors-as-critical(no)
        )
        url("http://127.0.0.1:8888")
    );
};

log {
    source(s_network);
    destination(d_http);
    flags(flow-control);
};
```
</details>


