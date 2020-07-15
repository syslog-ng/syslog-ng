scratch-buffers: fix `global.scratch_buffers_bytes.queued` counter bug
This bug only affected the stats_counter value, not the actual memory usage (i.e. memory usage was fine before)
