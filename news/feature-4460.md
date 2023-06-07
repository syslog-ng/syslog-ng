Added 2 new options

- `quote_char` to aid custom quoting for table and index names (e.g. MySQL needs sometimes this for certain identifiers)
**NOTE**: Using a back-tick character needs a special formatting as syslog-ng uses it for configuration parameter names, so for that use:     `quote_char("``")`  (double back-tick)
- `dbi_driver_dir` to define an optional DBI driver location for DBD initialization

NOTE: libdbi and libdbi-drivers OSE forks are updated, `afsql` now should work nicely both on ARM and X86 macOS systems too (tested on macOS 13.3.1 and 12.6.4)

Please do not use the pre-built ones (e.g. 0.9.0 from Homebrew), build from the **master** of the following
- https://github.com/balabit-deps/libdbi-drivers
- https://github.com/balabit-deps/libdbi
