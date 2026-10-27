`file()`, `wildcard-file()`: Fixed undetected file replacement or truncation when using
`follow-method(system-poll)`. Previously the reader kept watching the stale, deleted
inode instead of following the newly created file.
