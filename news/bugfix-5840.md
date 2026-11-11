`disk-buffer()`: use the whole queue file name range

A directory holding 10000 queue files refused further allocation and the destination ran with no disk buffer, although the file name format addresses 100000 names. The limit is now 100000, and the error message names the directory and the limit.
