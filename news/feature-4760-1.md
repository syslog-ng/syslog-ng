`pihole-ftl()`: Added a new source, which reads Pi-hole FTL logs.

The FTL (Faster Than Light) logs are the logs which can be found
in the "Tools" / "Pi-hole diagnosis" menu.

Example minimal config:
```
source s_pihole_ftl {
  pihole-ftl();
};
```

By default it reads the `/var/log/pihole/FTL.log` file.
You can change the root dir of Pi-hole's logs with the `dir()` option,
where the `FTL.log` file can be found.

As the `pihole-ftl()` source is based on a `file()` source, all of the
`file()` source options are applicable, too.
