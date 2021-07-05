kafka-c: sync_send(yes) enables synchronous message delivery, reducing the possibility of message loss.

```
kafka-c(
  bootstrap-server("localhost:9092")
  topic("syslog-ng")
  sync_send(yes)
);
```

Warning: this option also reduces significantly the performance of kafka-c driver.

