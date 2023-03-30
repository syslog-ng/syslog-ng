destination: Introduced queue metrics.

  * The corresponding driver is identified with the "id" and "driver_instance" labels.
  * Available counters are "memory_usage_bytes" and "events".
  * Memory queue metrics are available with "syslogng_memory_queue_" prefix,
    `disk-buffer` metrics are available with "syslogng_disk_queue_" prefix.
  * `disk-buffer` metrics have an additional "path" label, pointing to the location of the disk-buffer file
    and a "reliable" label, which can be either "true" or "false".
  * Threaded destinations, like `http`, `python`, etc have an additional "worker" label.

Example metrics
```
syslogng_disk_queue_events{driver_instance="http,http://localhost:1239",id="d_http_disk_buffer#0",path="/var/syslog-ng/syslog-ng-00000.rqf",reliable="true",worker="0"} 80
syslogng_disk_queue_events{driver_instance="http,http://localhost:1239",id="d_http_disk_buffer#0",path="/var/syslog-ng/syslog-ng-00001.rqf",reliable="true",worker="1"} 7
syslogng_disk_queue_events{driver_instance="http,http://localhost:1239",id="d_http_disk_buffer#0",path="/var/syslog-ng/syslog-ng-00002.rqf",reliable="true",worker="2"} 7
syslogng_disk_queue_events{driver_instance="http,http://localhost:1239",id="d_http_disk_buffer#0",path="/var/syslog-ng/syslog-ng-00003.rqf",reliable="true",worker="3"} 7
syslogng_disk_queue_events{driver_instance="tcp,localhost:1235",id="d_network_disk_buffer#0",path="/var/syslog-ng/syslog-ng-00000.qf",reliable="false"} 101
syslogng_disk_queue_memory_usage_bytes{driver_instance="http,http://localhost:1239",id="d_http_disk_buffer#0",path="/var/syslog-ng/syslog-ng-00000.rqf",reliable="true",worker="0"} 3136
syslogng_disk_queue_memory_usage_bytes{driver_instance="http,http://localhost:1239",id="d_http_disk_buffer#0",path="/var/syslog-ng/syslog-ng-00001.rqf",reliable="true",worker="1"} 2776
syslogng_disk_queue_memory_usage_bytes{driver_instance="http,http://localhost:1239",id="d_http_disk_buffer#0",path="/var/syslog-ng/syslog-ng-00002.rqf",reliable="true",worker="2"} 2760
syslogng_disk_queue_memory_usage_bytes{driver_instance="http,http://localhost:1239",id="d_http_disk_buffer#0",path="/var/syslog-ng/syslog-ng-00003.rqf",reliable="true",worker="3"} 2776
syslogng_disk_queue_memory_usage_bytes{driver_instance="tcp,localhost:1235",id="d_network_disk_buffer#0",path="/var/syslog-ng/syslog-ng-00000.qf",reliable="false"} 39888
syslogng_memory_queue_events{driver_instance="http,http://localhost:1236",id="d_http#0",worker="0"} 15
syslogng_memory_queue_events{driver_instance="http,http://localhost:1236",id="d_http#0",worker="1"} 14
syslogng_memory_queue_events{driver_instance="tcp,localhost:1234",id="d_network#0"} 29
syslogng_memory_queue_memory_usage_bytes{driver_instance="http,http://localhost:1236",id="d_http#0",worker="0"} 5896
syslogng_memory_queue_memory_usage_bytes{driver_instance="http,http://localhost:1236",id="d_http#0",worker="1"} 5552
syslogng_memory_queue_memory_usage_bytes{driver_instance="tcp,localhost:1234",id="d_network#0"} 11448
```
