file-source: new option to multi-line file sources: `multi-line-timeout()`

After waiting multi-line-timeout() seconds without reading new data from the file, the last (potentially partial) message will be flushed and sent through the pipeline as a LogMessage.
Since the multi-line file source detects the end of a message after finding the beginning of the subsequent message (indented or no-garbage/suffix mode), this option can be used to flush the last multi-line message in the file after a multi-line-timeout()-second timeout.

There is no default value, i.e. this timeout needs to be explicitly configured.

Example config:
```
file("/some/folder/events"
    multi-line-mode("prefix-garbage")
    multi-line-prefix('^EVENT: ')
    multi-line-timeout(10)
    flags("no-parse")
);
```
