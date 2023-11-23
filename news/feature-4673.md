`$TRANSPORT`: this is a new name-value pair that syslog-ng populates
automatically.  It indicates the "transport" mechanism used to
retrieve/receive the message.  It is up to the source driver to determine
the value. Currently the following values were implemented:

  BSD syslog drivers: `tcp()`, `udp()` & `network()`
  * rfc3164+tls
  * rfc3164+tcp
  * rfc3164+udp
  * rfc3164+proxied-tls
  * rfc3164+<custom logproto like altp>

  UNIX domain drivers: `unix-dgram()`, `unix-stream()`
  * unix-stream
  * unix-dgram

  RFC5424 style syslog: `syslog()`:
  * rfc5426: syslog over udp
  * rfc5425: syslog over tls
  * rfc6587: syslog over tcp
  * rfc5424+<custom logproto like altp>: syslog over a logproto plugin

  Other drivers:
  * otlp: `otel()` driver
  * mqtt: `mqtt()` driver
  * hypr-api: `hypr-audit-source()` driver


`$IP_PROTO`: indicate the IP protocol version used to retrieve/receive the
message. Contains either "4" to indicate IPv4 and "6" to indicate IPv6.
