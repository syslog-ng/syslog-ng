`disk-buffer()`: the default value of the following options has been changed for performance reasons:
  - truncate-size-ratio(): from 0.01 to 0.1 (from 1% to 10%)
  - qout-size(): from 64 to 1000 (this affects only the non-reliable disk buffer)
