The `json-c` library is no longer bundled in the syslog-ng source tarball

Since all known OS package managers provide json-c packages nowadays, the json-c
submodule has been removed from the source tarball.

The `--with-jsonc=internal` option of the `configure` script has been removed
accordingly, system libraries will be used instead. For special cases, the JSON
support can be disabled by specifying `--with-jsonc=no`.
