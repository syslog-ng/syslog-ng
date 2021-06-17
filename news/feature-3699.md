kafka-c: batching support in case of sync_send(yes)

```
kafka-c(
 bootstrap-server("localhost:9092")
 topic("syslog-ng")
 sync_send(yes)
 batch_lines(10)
 btach_timeout(10000)
);
```

Note1: batch_lines are accepted in case of sync_send(no), but no batching is done.
Note2: messages are still sent one at a time to kafka, the batch yields multiple message per transaction.
