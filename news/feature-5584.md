`cloud-auth`, `grpc`: Add OAuth2 support for gRPC destinations

The `cloud-auth()` plugin now supports gRPC destinations in addition to HTTP
destinations. This enables OAuth2 token management for any gRPC-based
destination driver (such as `bigquery()`, `opentelemetry()`, `loki()`, etc.).

The implementation uses the same signal/slot pattern as HTTP.

Example configuration:

```
destination d_grpc {
  opentelemetry(
    url("example.com:443")
    cloud-auth(
      oauth2(
        client_id("client-id")
        client_secret("client-secret")
        token_url("https://auth.example.com/token")
        scope("api-scope")
      )
    )
  );
}
```

