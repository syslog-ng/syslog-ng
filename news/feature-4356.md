`disk-buffer`: Added capacity, disk_allocated and disk_usage metrics.

  * "capacity_bytes": The theoretical maximal useful size of the disk-buffer.
                      This is always smaller, than `disk-buf-size()`, as there is some reserved
                      space for metadata. The actual full disk-buffer file can be larger than this,
                      as syslog-ng allows to write over this limit once, at the end of the file.

  * "disk_allocated_bytes": The current size of the disk-buffer file on the disk. Please note that
                            the disk-buffer file size does not strictly correlate with the number
                            of messages, as it is a ring buffer implementation, and also syslog-ng
                            optimizes the truncation of the file for performance reasons.

  * "disk_usage_bytes": The serialized size of the queued messages in the disk-buffer file. This counter
                        is useful for calculating the disk usage percentage (disk_usage_bytes / capacity_bytes)
                        or the remaining available space (capacity_bytes - disk_usage_bytes).

Example metrics:
```
syslogng_disk_queue_capacity_bytes{driver_id="d_network#0",driver_instance="tcp,localhost:1235",path="/var/syslog-ng-00000.rqf",reliable="true"} 104853504
syslogng_disk_queue_disk_allocated_bytes{driver_id="d_network#0",driver_instance="tcp,localhost:1235",path="/var/syslog-ng-00000.rqf",reliable="true"} 17284
syslogng_disk_queue_disk_usage_bytes{driver_id="d_network#0",driver_instance="tcp,localhost:1235",path="/var/syslog-ng-00000.rqf",reliable="true"} 13188
```
