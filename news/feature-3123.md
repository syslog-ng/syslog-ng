
The `python-http-header` plugin makes it possible for users to implement HTTP header plugins in Python language. It is built on top of signal-slot mechanism: currently HTTP module defines only one signal, that is `signal_http_header_request` and `python-http-header` plugin implements a python binding for this signal. This means that when the `signal_http_header_request` signal is emitted then the connected slot executes the Python code.

The Python interface is :
```
def get_headers(self, body, headers):
```

It should return a tuple that has two elements:
 * a timeout (when the signal is emitted again within `timeout`,  a cached list is returned - this can be used for caching Authorization header tokens that has an expiration time and as the check of the value of the timeout is done at C site, it can improve the performance: Python code runs only when it is required) 
 * a string list (the headers that will be appended to the request's header)

Code, or naming is not final, but it's working. Just wanted to open the PR and get some feedback (...and run CI tests).

Original code was written by Ferenc Sipos.

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
        self.timeout = 0
        logger.debug(f"TestCounter class instantiated; options={options}")

    def get_headers(self, body, headers):
        logger.debug(f"get_headers() called, received body={body}, headers={headers}")
       
        response = (self.timeout, [f"{self.header}: {self.counter}"])
        self.counter += 1
        return response

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


