file-dest: add new feature: symlink-as

This feature allows one to maintain a persistent symlink to a log file when a
template is used (for example: `/var/log/cron -> /var/log/cron.${YEAR}${MONTH}`).

Configuration looks like this:

```
destination d_file_cron {
  file("/var/log/cron.${YEAR}${MONTH}" symlink-as("/var/log/cron"));
};
```

From a functional perspective, the `symlink-as` file inherits both
`create-dirs` and file ownership from its file destination (permissions are not
applicable to symlinks, at least on linux).

The symlink is adjusted at the time a new destination file is opened (in the
example above, if `${YEAR}` or `${MONTH}` changes).

Although not specific to time macros, that's where the usefulness is. If the
template contains something like $PROGRAM or $HOST, the configuration wouldn't
necessarily be invalid, but you'd get an ever-changing symlink of dubious
usefulness.
