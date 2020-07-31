`internal()`:  limit the size of internal()'s temporary queue

The `internal()` source uses a temporary queue to buffer messages.
From now on, the queue has a maximum capacity, the `log-fifo-size()` option
can be used to change the default limit (10000).

This change prevents consuming all the available memory in special rare cases.
