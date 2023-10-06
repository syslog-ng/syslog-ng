#### Sending messages to Google Pub/Sub

The `google-pubsub()` destination feeds Google Pub/Sub via the [HTTP REST API](https://cloud.google.com/pubsub/docs/reference/rest/v1/projects.topics/publish).

Example config:
```
google-pubsub(
  project("syslog-ng-project")
  topic("syslog-ng-topic")
  auth(
    service-account(
      key("/path/to/service-account-key.json")
    )
  )
);
```

See the [Google Pub/Sub documentation](https://cloud.google.com/pubsub/docs/building-pubsub-messaging-system) to learn more about configuring a service account.
