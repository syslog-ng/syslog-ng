secure-logging: improvements

  - removed 1500 message length limitation
  - `slogimport` has been renamed to `slogencrypt`
  - `$(slog)` will not start anymore when key is not found
  - internal messaging (warning, debug) improvements
  - improved memory handling and error information display
  - CMake build improvements
  - switched to GLib command line argument parsing
  - the output of `slogkey -s` is now parsable
  - manpage improvements
