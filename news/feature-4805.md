`http()`: Added a new counter for HTTP requests.

It is activated on `stats(level(1));`.

Example metrics:
```
syslogng_output_http_requests_total{url="http://localhost:8888/bar",response_code="200",driver="http",id="#anon-destination0#0"} 16
syslogng_output_http_requests_total{url="http://localhost:8888/bar",response_code="401",driver="http",id="#anon-destination0#0"} 2
syslogng_output_http_requests_total{url="http://localhost:8888/bar",response_code="502",driver="http",id="#anon-destination0#0"} 1
syslogng_output_http_requests_total{url="http://localhost:8888/foo",response_code="200",driver="http",id="#anon-destination0#0"} 24
```
