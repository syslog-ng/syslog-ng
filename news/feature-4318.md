`metrics-probe()`: Added a new parser which counts messages passing through based on the metadata of each message.

`metrics-probe()` creates labeled stats counters based on the fields of the message.

Both the key and labels can be set in the config, the values of the labels can be templated. E.g.:
```
parser p_metrics_probe {
  metrics-probe(
    key("custom_key")  # adds "syslogng_" prefix => "syslogng_custom_key"
    labels(
      "custom_label_name_1" => "foobar"
      "custom_label_name_2" => "${.custom.field}"
    )
  );
};
```
With this config, it creates counters like these:
```
syslogng_custom_key{custom_label_name_1="foobar", custom_label_name_2="bar"} 1
syslogng_custom_key{custom_label_name_1="foobar", custom_label_name_2="foo"} 1
syslogng_custom_key{custom_label_name_1="foobar", custom_label_name_2="baz"} 3
```

The minimal config creates counters with the key `syslogng_classified_events_total`
and labels `app`, `host`, `program` and `source`. E.g.:
```
parser p_metrics_probe {
  metrics-probe();

  # Same as:
  #
  # metrics-probe(
  #   key("classified_events_total")
  #   labels(
  #     "app" => "${APP}"
  #     "host" => "${HOST}"
  #     "program" => "${PROGRAM}"
  #     "source" => "${SOURCE}"
  #   )
  # );
};
```
With this config, it creates counters like these:
```
syslogng_classified_events_total{app="example-app", host="localhost", program="baz", source="s_local_1"} 3
syslogng_classified_events_total{app="example-app", host="localhost", program="bar", source="s_local_1"} 1
syslogng_classified_events_total{app="example-app", host="localhost", program="foo", source="s_local_1"} 1
```

This feature is experimental; the counters created by `metrics-probe()`
(names, labels, etc.) may change in the next 2-3 releases.
