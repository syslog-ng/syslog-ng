`mqtt()` source: Added `${MQTT_TOPIC}` name-value pair.

It is useful for the cases where `topic()` contains wildcards.

Example config:
```
log {
  source { mqtt(topic("#")); };
  destination { stdout(template("${MQTT_TOPIC} - ${MESSAGE}\n")); };
};
```
