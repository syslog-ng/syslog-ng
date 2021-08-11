`disk-buffer`: Added a new option to reliable disk-buffer: `qout-size()`.

This option sets the number of messages that are stored in the memory in addition
to storing them on disk. The default value is 1000.

This serves performance purposes and offers the same no-message-loss guarantees as
before.

It can be used to maintain a higher throughput when only a small number of messages
are waiting in the disk-buffer.
