`logmsg`: Added dynamic name-value registry controls.

New `dynamic-nv-name-limit()` and `dynamic-nv-name-overflow()` global options limit new dynamic field names.
The current registry size is exposed through `logmsg_nv_registry_size` and `logmsg_dynamic_nv_registry_size` stats counters.
