Fix flow-control when `fetch-limit()` is set higher than 64K

In high-performance use cases, users may configure log-iw-size() and
fetch-limit() to be higher than 2^16, which caused flow-control issues,
such as messages stuck in the queue forever or log sources not receiving
messages.
