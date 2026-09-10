`http`: Fixed a bug where a message retried after a critical header callback failure was appended to the
already-finished request body. All per-request state is now reset before returning on such errors.
