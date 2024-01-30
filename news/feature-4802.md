### Collecting Jellyfin logs

The new `jellyfin()` source, reads Jellyfin logs from its log file output.

Example minimal config:
```
source s_jellyfin {
  jellyfin(
    base-dir("/path/to/my/jellyfin/root/log/dir")
    filename-pattern("log_*.log")
  );
};
```

For more details about Jellyfin logging, see:
 * https://jellyfin.org/docs/general/administration/configuration/#main-configuration
 * https://jellyfin.org/docs/general/administration/configuration/#log-directory

As the `jellyfin()` source is based on a `wildcard-file()` source, all of the
`wildcard-file()` source options are applicable, too.
