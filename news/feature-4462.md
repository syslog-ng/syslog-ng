splunk: Added support for sending messages to Splunk HEC.

The `splunk-hec-event()` destination feeds Splunk via the [HEC events API](https://docs.splunk.com/Documentation/Splunk/9.0.4/RESTREF/RESTinput#services.2Fcollector.2Fevent.2F1.0).

Minimal config:
```
destination d_splunk_hec_event {
  splunk-hec-event(
    url("https://localhost:8088")
    token("70b6ae71-76b3-4c38-9597-0c5b37ad9630")
  );
};
```

Additional options include:
  * `event()`
  * `index()`
  * `source()`
  * `sourcetype()`
  * `host()`
  * `time()`
  * `default-index()`
  * `default-source()`
  * `default-sourcetype()`
  * `fields()`
  * `extra-headers()`
  * `extra-queries()`
  * `content-type()`

`event()` accepts a template, which declares the content of the log message sent to Splunk.
By default we send the `${MSG}` value.

`index()`, `source()`, `host()` and `time()` accepts templates, which declare the respective field
of each log message based on the set template.

`default-index()`, `default-source()` and `default-sourcetype()` accepts literal strings, which
will be used for fallback values if a log message does not set these fields. These values are
passed to the URL as query parameters, so they do not inflate the body of the HTTP request
for each message in the batch, which saves bandwidth.

`fields()` accepts template, which is passed as additional indexing metadata to Splunk.

`extra-headers()`, `extra-queries()` and `content-type()` are additional HTTP request options.

The `splunk-hec-raw()` destination feeds Splunk via the [HEC raw API](https://docs.splunk.com/Documentation/Splunk/9.0.4/RESTREF/RESTinput#services.2Fcollector.2Fraw.2F1.0).

Regarding its options, it is similar to the `splunk-hec-event()` destination with the addition
of the mandatory `channel()` option, which accepts a GUID as a literal string that differentiates
data from different clients. Another difference is that instead of `event()`, here the `template()`
option sets the template which will represent the log message in Splunk.

Minimal config:
```
destination d_splunk_hec_raw {
  splunk-hec-raw(
    url("https://localhost:8088")
    token("70b6ae71-76b3-4c38-9597-0c5b37ad9630")
    channel("05ed4617-f186-4ccd-b4e7-08847094c8fd")
  );
};
```
