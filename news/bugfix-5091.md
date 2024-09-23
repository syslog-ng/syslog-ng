`wildcard-file()`: fix crashes can occure if the same wildcard file is used in multiple sources

Because of some persistent name construction and validation bugs the following config crashed `syslog-ng`
(if there were more than one log file is in the `/path` folder)

``` config
@version: current

@include "scl.conf"

source s_files1 {
    file("/path/*.log"
        persist-name("p1")
    );
};

source s_files2 {
    file("/path/*.log"
        persist-name("p2")
    );
};

destination s_stdout {
    stdout();
};

log {
    source(s_files1);
    destination(s_stdout);
};

log {
    source(s_files2);
    destination(s_stdout);
};
```

NOTE:

- The issue occurred regardless of the presence of the `persist-name()` option.
- It affected not only the simplified example of the legacy wildcard `file()` but also the new `wildcard-file()` source.
