# Threaded diskq source

This example source plugin reads messages from a `disk-buffer` file.

The driver opens a `disk-buffer` file in read-only mode and forwards its entire content.
After all messages have been read, the source starts to check for a new file periodically (`time-reopen()`).

You can remove the processed queue file or move a new one to the specified location (`file()`).

If you reload/restart syslog-ng, the source will resend everything from the beginning.

Note that this is a sample plugin, it is meant to be used only in debugging environments.

### Usage

```
source s_diskq {
  diskq-source(file("/var/disk-buffers/syslog-ng-00000.rqf"));
};

log {
  source(s_diskq);

  destination {
    file("/dev/stdout");
  };
};
```
