`wildcard-file()`: fix infrequent crash when file renamed/recreated

The wildcard-file source crashed when a file being processed was replaced by
a new one on the same path (renamed, deleted+recreated, rotated, etc.).
