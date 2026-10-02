`system-source`: Fixed parsing of FreeBSD kernel messages from `system()`.

Kernel messages with colons in their payload no longer lose their prefix during parsing.