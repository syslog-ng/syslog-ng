`disk-buffer`: added the `remove-if-empty()` option, which removes a queue file and its persist entry when the
destination that owns it is torn down by `time-reap()` and the queue holds no messages. It defaults to `no`, and it is
meant for templated destinations whose expansions never come back, such as a path built from a receive-time date macro,
where every date roll would otherwise leave queue files behind permanently.
