`disk-buffer`: remove empty queue files when their destination is reaped

A disk-buffer file whose destination has been torn down by `time-reap()` is now removed, together with its persist
entry, when it holds no messages. Previously every expansion of a templated destination left a queue file behind
permanently, which could exhaust the 10000 entry filename namespace on a long-running instance.
