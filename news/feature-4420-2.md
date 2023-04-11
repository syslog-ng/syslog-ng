syslog-ng configuration identifier

A new syslog-ng configuration keyword has been added, which allows specifying a config identifier. For example:
```
@config-id: cfg-20230404-13-g02b0850fc
```

This keyword can be used for config identification in managed environments, where syslog-ng instances and their
configuration are deployed/generated automatically.

`syslog-ng-ctl config --id` can be used to query the active configuration ID and the SHA256 hash of the full
"preprocessed" syslog-ng configuration. For example:

```
$ syslog-ng-ctl config --id
cfg-20230404-13-g02b0850fc (08ddecfa52a3443b29d5d5aa3e5114e48dd465e195598062da9f5fc5a45d8a83)
```
