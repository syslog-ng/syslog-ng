`qbittorrent()`: Added a new source, which reads qBittorrent logs.

Example minimal config:
```
source s_qbittorrent {
  qbittorrent(
    dir("/path/to/my/qbittorrent/root/log/dir")
  );
};
```

The root dir of the qBittorrent logs can be found in the
"Tools" / "Preferences" / "Behavior" / "Log file" / "Save path" field.

As the `qbittorrent()` source is based on a `file()` source, all of the `file()`
source options are applicable, too.
