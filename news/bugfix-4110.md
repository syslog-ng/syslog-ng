Fixed buffer handling of syslog and timestamp parsers

Multiple buffer out-of-bounds issues have been fixed, which could cause
hangs, high CPU usage, or other undefined behavior.
