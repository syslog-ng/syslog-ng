### Collecting *arr logs

Use the newly added `*arr()` sources to read various *arr logs:
  * `lidarr()`
  * `prowlarr()`
  * `radarr()`
  * `readarr()`
  * `sonarr()`
  * `whisparr()`

Example minimal config:
```
source s_radarr {
  radarr(
    dir("/path/to/my/radarr/log/dir")
  );
};
```

The logging module is stored in the `<prefix><module>` name-value pair,
for example: `.radarr.module` => `ImportListSyncService`.
The prefix can be modified with the `prefix()` option.
