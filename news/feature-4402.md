`disk-buffer`: Added metrics for abandoned disk-buffer files.

Availability is the same as the `disk_queue_dir_available_bytes` metric.

Example metrics:
```
syslogng_disk_queue_capacity_bytes{abandoned="true",path="/var/syslog-ng/syslog-ng-00000.rqf",reliable="true"} 104853504
syslogng_disk_queue_disk_allocated_bytes{abandoned="true",path="/var/syslog-ng/syslog-ng-00000.rqf",reliable="true"} 273408
syslogng_disk_queue_disk_usage_bytes{abandoned="true",path="/var/syslog-ng/syslog-ng-00000.rqf",reliable="true"} 269312
syslogng_disk_queue_events{abandoned="true",path="/var/syslog-ng/syslog-ng-00000.rqf",reliable="true"} 860
```
