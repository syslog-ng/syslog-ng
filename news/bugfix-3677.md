`mongodb()`: fix crash with older mongo-c-driver versions

syslog-ng crashed (was aborted) when the `mongodb()` destination was used with
older mongo-c-driver versions (< v1.11.0).
