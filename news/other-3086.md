`http`: `use-system-cert-store` functionality has been changed

This option was added in order to support monolithic installations
that ships even libcurl... But later, as it came out, it's not enough.
What we really need is to do some autodetection regarding ca-file and ca-dir.
In monolithic installations we enable this by default in the related SCLs.

