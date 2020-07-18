systemd-journal: add namespace() option
This option accepts a string which is identical to the `--namespace` option of journalctl.
For systems defining this option with a `systemd` version older than `v245` a warning is issued.