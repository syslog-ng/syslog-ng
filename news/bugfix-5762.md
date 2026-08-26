`disk-buffer`: fixed a file descriptor leak in the disk-buffer file allocator. Every disk-buffer file created leaked
one descriptor on the spool directory's lock file for the lifetime of the process, so a configuration with
high-cardinality templated destinations could exhaust the process descriptor limit. Present since 4.2.0.
