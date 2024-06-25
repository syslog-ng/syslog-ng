### Send log messages to Elasticsearch data stream
The `elasticsearch-datastream()` destination can be used to feed Elasticsearch [data streams](https://www.elastic.co/guide/en/elasticsearch/reference/current/data-streams.html).

Example config:

```
elasticsearch-datastream(
  url("https://elastic-endpoint:9200/my-data-stream/_bulk")
  user("elastic")
  password("ba3DI8u5qX61We7EP748V8RZ") 
);
```
