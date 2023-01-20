`syslog-parser()` and all syslog related sources: accept unquoted RFC5424
SD-PARAM-VALUEs instead of rejecting them with a parse error.

`sdata-parser()`: this new parser allows you to parse an RFC5424 style
structured data string. It can be used to parse this relatively complex
format separately.
