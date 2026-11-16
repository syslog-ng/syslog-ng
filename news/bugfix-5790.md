`transport`: Fixed a PROXY protocol header announcing an IPv6 (TCP6) address that could abort
syslog-ng when built without IPv6 support. The connection is now kept alive using the real
endpoint address, logging a warning instead.
