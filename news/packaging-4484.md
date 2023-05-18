C++ plugins: Some of syslog-ng's plugins now contain C++ code.

By default they are being built if a C++ compiler is available.
Disabling it is possible with `--disable-cpp`.

Affected plugins:
  * `lib/syslog-ng/libexamples.so`
    * `--disable-cpp` will only disable the C++ part (`random-choice-generator()`)
