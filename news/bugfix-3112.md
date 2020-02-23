`graylog2`, `format-gelf`: Fixed sending empty message, when ${PID} is not set.
Also added a default value "-" to empty `short_message` and `host` as they are mandatory fields.
