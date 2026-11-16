`logmsg`: Fixed a potential heap corruption when deserializing a corrupted or tampered
disk-buffer record whose tag-name length field was `0xFFFFFFFF`. This value could free an
internal thread-local scratch buffer without invalidating it, corrupting state relied upon
by later records processed by the same thread. Also fixed a related bookkeeping
inconsistency left behind after reallocating that buffer, and a scratch-buffer resource
leak on the rejected-input path.
