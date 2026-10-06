`logproto`: Fixed a `guint32` wraparound in the RFC6587 octet-counting ("framed" transport)
frame-length parser. A declared frame length of 4294967296 or higher was silently reduced
modulo 2^32 instead of being rejected, letting a single oversized frame be reinterpreted as
multiple, differently-bounded messages.
