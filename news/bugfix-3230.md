`affile`: eliminate infinite loop in case of a spurious file path

If the template evaluation of a log message will result to a spurious
path in the file destination, syslog-ng refuses to create that file.
However the problematic log message was left in the msg queue, so
syslog-ng was trying to create that file again in time-reopen periods.
From now on syslog-ng will handle "permanent" file errors, and drop
the relevant msg.
