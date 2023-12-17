`cloud-auth()`: Added support for `user-managed-service-account()` `gcp()` auth method.

This authentication method can be used on VMs in GCP to use the linked service.

Example minimal config, which tries to use the "default" service account:
```
cloud-auth(
  gcp(
    user-managed-service-account()
  )
)
```

Full config:
```
cloud-auth(
  gcp(
    user-managed-service-account(
      name("alltilla@syslog-ng-test-project.iam.gserviceaccount.com")
      metadata-url("my-custom-metadata-server:8080")
    )
  )
)
```

This authentication method is extremely useful with syslog-ng's `google-pubsub()` destination,
when it is running on VMs in GCP, for example:
```
destination {
  google-pubsub(
    project("syslog-ng-test-project")
    topic("syslog-ng-test-topic")
    auth(user-managed-service-account())
  );
};
```

For more info about this GCP authentication method, see:
 * https://cloud.google.com/compute/docs/access/authenticate-workloads#curl
 * https://cloud.google.com/compute/docs/access/create-enable-service-accounts-for-instances
