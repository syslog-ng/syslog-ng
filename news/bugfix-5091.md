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

**<u>WARNING</u>**, the main issue causing this problem was that the generated final persist names for the
`file()` and `wildcard-file()` sources were not always unique. To fix this, we had to change the format of
the names used in the persist files. These changes will result in the loss of persist information in the
`syslog-ng` persist files!

**<u>BE AWARE</u>**, ensure that all your existing wildcard `file()` and `wildcard-file()` sources are fully processed before
installing the new version of `syslog-ng`. Otherwise, the content of those files will be reprocessed from the
beginning at the next startup of the updated `syslog-ng`, as the file read positions in the persist files will be
ignored due to the changed persist names.
