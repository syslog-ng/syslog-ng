`logproto`: Fixed a potential crash when a source has `encoding()` set and `log-msg-size()` is configured large
enough to overflow the io-worker thread's stack. The raw, pre-conversion read buffer used to be allocated
directly on the stack, sized from `log-msg-size()` with no upper bound, regardless of the actual message
content; it is now allocated on the heap instead, growing on demand and reused for the lifetime of the
connection.
