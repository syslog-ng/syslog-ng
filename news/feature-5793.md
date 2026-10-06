`disk-buffer`: Reliable disk-queues now quarantine themselves after repeatedly crashing while
processing the same unacked record. If the same read position survives 3 restarts in a row without
being acked, the queue file is renamed to `.corrupted` and a fresh one is started, the same way an
already-corrupted file is handled, instead of replaying the same poison record forever.
