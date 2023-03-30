`disk-buffer`: Added metrics for monitoring the available space in disk-buffer `dir()`s.

Metrics are available from `stats(level(1))`.

By default, the metrics are generated every 5 minutes, but it can be changed in the global options:
```
options {
  disk-buffer(
    stats(
      freq(10)
    )
  );
};
```
Setting `freq(0)` disabled this feature.

Example metrics:
```
syslogng_disk_queue_dir_available_bytes{dir="/var/syslog-ng"} 870109413376
```
