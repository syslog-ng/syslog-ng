metrics: add syslogng_memory_queue_capacity

Shows the capacity (maximum possible size) of each queue.
Note that this metric publishes `log-fifo-size()`, which only limits non-flow-controlled messages.
Messages coming from flow-controlled paths are not limited by `log-fifo-size()`, their corresponding
source `log-iw-size()` is the upper limit.
