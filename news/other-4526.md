`disk-buffer`: Added alternative option names

`disk-buf-size()` -> `capacity-bytes()`
`qout-size()` -> `front-cache-size()`
`mem-buf-length()` -> `flow-control-window-size()`
`mem-buf-size()` -> `flow-control-window-bytes()`

Old option names are still available.

Example configs:
```
tcp(
  "127.0.0.1" port(2001)
  disk-buffer(
    reliable(yes)
    capacity-bytes(1GiB)
    flow-control-window-bytes(200MiB)
    front-cache-size(1000)
  )
);

tcp(
  "127.0.0.1" port(2001)
  disk-buffer(
    reliable(no)
    capacity-bytes(1GiB)
    flow-control-window-size(10000)
    front-cache-size(1000)
  )
);
```
