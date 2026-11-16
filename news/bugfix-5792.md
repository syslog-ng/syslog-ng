`logmsg`: Fixed a `guint16` wraparound in the per-message NVTable dynamic field index. A single
message carrying 65536 or more distinct dynamic fields (via SDATA, key-value, or JSON parsing)
could silently lose all of its previously parsed fields once the counter wrapped back to zero.
The index is now capped instead of wrapping.
