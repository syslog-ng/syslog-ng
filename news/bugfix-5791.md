`logmsg`: Fixed a crash when deserializing a corrupted or tampered disk-buffer record whose
structured-data handle did not correspond to any registered name, a potential heap buffer
overflow when such a record's `sdata` entry count exceeded its declared allocation, and
uninitialized NV handles left behind by the post-deserialization handle fixup for entries
it could not resolve.
