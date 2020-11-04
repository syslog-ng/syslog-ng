file, network, program destinations: : new truncate_size option introduced to truncate an output message to a specified max size. default value is -1 (disabled).

```
network("127.0.0.1" truncate_size(100));
```

new stats counters:
```
dst.network;d_local#0;udp,127.0.0.1:1111;a;truncated_count;1
dst.network;d_local#0;udp,127.0.0.1:1111;a;truncated_bytes;1
```
