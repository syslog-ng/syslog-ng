Sending messages to CrowdStrike Falcon LogScale (Humio)

The `logscale()` destination feeds LogScale via the [Ingest API](https://library.humio.com/falcon-logscale/api-ingest.html#api-ingest-structured-data).

Minimal config:
```
destination d_logscale {
  logscale(
    token("my-token")
  );
};
```
Additional options include:
  * `url()`
  * `rawstring()`
  * `timestamp()`
  * `timezone()`
  * `attributes()`
  * `extra-headers()`
  * `content-type()`


__TODO: Add Andreas Friedmann and Ryan Faircloth to Contributors!!!!!__