`cloud-auth()`: Added a new plugin for drivers, which implements different cloud related authentications.

Currently the only supported authentication is [GCP's Service Account](https://cloud.google.com/iam/docs/service-account-overview) for the `http()` destination.

Example config:
```
http(
  cloud-auth(
    gcp(
      service-account(
        key("/path/to/service-account-key.json")
        audience("https://pubsub.googleapis.com/google.pubsub.v1.Publisher")
      )
    )
  )
);
```
