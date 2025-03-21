`google-pubsub-grpc()`: Added a new destination that sends logs to Google Pub/Sub via the gRPC interface.

Example config:
```
google-pubsub-grpc(
  project("my_project")
  topic($topic)

  data($MESSAGE)
  attributes(
    timestamp => $S_ISODATE,
    host => $HOST,
  )

  workers(4)
  batch-timeout(1000) # ms
  batch-lines(1000)
);
```

The `project()` and `topic()` options are templatable.
The default service endpoint can be changed with the `service_endpoint()` option.
