`options`: Added the `stack-alloc-limit()` global option. It lets users opt back into a faster,
stack-based allocation for internal buffers whose size is derived from a configurable option (such as
`log-msg-size()`), for buffers up to the configured limit. Defaults to `0`, meaning always use the heap;
only raise this after validating your deployment's actual thread stack size, see the admin guide for the
trade-offs.
