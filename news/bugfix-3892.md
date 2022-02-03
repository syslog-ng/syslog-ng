`file()` source: fixed invalid buffer handling when `encoding()` is used

A bug has been fixed that - under rare circumstances - could cause message
duplication or partial message loss when non-fixed length or less known
fixed-length encodings are used.
