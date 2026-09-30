`syslog-format`: Fixed a potential crash when parsing an invalid UTF-8 message with `flags(sanitize-utf8)` enabled.
The sanitizer used to allocate a stack buffer sized directly from the (sender-controlled) message length; it now uses a bounded, pooled buffer regardless of message size.
