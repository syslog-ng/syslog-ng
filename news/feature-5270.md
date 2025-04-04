bigquery(), google-pubsub-grpc(): Added service-account() authentication option.

Example usage:
```
destination {
    google-pubsub-grpc(
        project("test")
        topic("test")
        auth(service-account(key ("path_to_service_account_key.json")))
    );
};
```

Note: In contrary to the `http()` destination's similar option,
we do not need to manually set the audience here as it is
automatically recognized by the underlying gRPC API.
