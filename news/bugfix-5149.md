`s3()`: Eliminated indefinite memory usage increase for each reload.

The increased memory usage is caused by the `botocore` library, which
caches the session information. We only need the Session object, if
`role()` is set. The increased memory usage still happens with that set,
currently we only fixed the unset case.
