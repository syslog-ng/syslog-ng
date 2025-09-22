4.10.0
======

syslog-ng provides [RPM](https://github.com/syslog-ng/syslog-ng#rhel) and [DEB](https://github.com/syslog-ng/syslog-ng#debianubuntu) package repositories for Ubuntu, Debian, and RHEL, for both amd64 and arm64 architectures.

We also provide ready-to-test binaries in [Docker containers](https://hub.docker.com/r/balabit/syslog-ng/tags) based on the current stable Debian image.

For more details, visit our [Documentation Center](https://syslog-ng.github.io/)

## Highlights

  * affile: add support for filesize based logrotation

    Config file example:
    ```
    destination d_file_logrotate {
        file("/my-logfile.log", logrotate(enable(yes), size(30KB), rotations(5)));
    };
    ```
    ([#5191](https://github.com/syslog-ng/syslog-ng/pull/5191))

## Features

  * filter: Add a `blank` filter that test if a value is blank.  A value is considered blank if it is not set, is an empty string, is an empty list, is a string with only whitespace, or is boolean false.
    ([#5425](https://github.com/syslog-ng/syslog-ng/pull/5425))

  * http-destination: add `msg_data_in_header` option to be able to turn off sending message content-related data in the HTTP header
    ([#5455](https://github.com/syslog-ng/syslog-ng/pull/5455))

  * stats-exporter: Add `stats-without-orphaned()` and `stats-with-legacy()` options to further filter the stats output.
    ([#5424](https://github.com/syslog-ng/syslog-ng/pull/5424))

  * affile: Add ability to refine the `wildcard-file()` `filename-pattern()` option with `exclude-pattern()`, to exclude some matching files. For example, match all `*.log` but exclude `*.?.log`.
    ([#5416](https://github.com/syslog-ng/syslog-ng/pull/5416))


## Bugfixes

  * afprog: Fix invalid access of freed log-writer cfg.
    ([#5445](https://github.com/syslog-ng/syslog-ng/pull/5445))

  * s3: Fixed bug where in certain conditions finished object buffers would fail to upload.
    ([#5447](https://github.com/syslog-ng/syslog-ng/pull/5447))

  * stats-exporter: Fix leaks caused by missing "virtual destructor" calls.
    ([#5441](https://github.com/syslog-ng/syslog-ng/pull/5441))

  * wildcard-file: Fix possible act_tracker crash caused by duplicated file change notification handler invocation.
    ([#5437](https://github.com/syslog-ng/syslog-ng/pull/5437))


## Packaging

  * packaging: New package formats, platforms, and architectures!

    - the long-awaited RPM repository is here, we have RHEL-8, RHEL-9, and REHL-10 packages available, both for amd64 and arm64 architectures,\
      just download and install the repository definition

        ``` shell
        sudo curl -o /etc/yum.repos.d/syslog-ng-ose-stable.repo https://ose-repo.syslog-ng.com/yum/syslog-ng-ose-stable.repo
        ```

    - we fixed the publishing of our arm64 DEB packages
    - added new DEB packages for Debian Trixie, both for amd64 and arm64.
    - new DBLD docker images for Rocky-9, OpenSuse Tumbleweed, Ubuntu Plucky, and Debian Trixie
    ([#5449](https://github.com/syslog-ng/syslog-ng/pull/5449))


## Notes to developers

  * mongo-c: now we can build against both the v1 and v2 versions of the mongo-c driver automatically
    ([#5442](https://github.com/syslog-ng/syslog-ng/pull/5442))

  * manpages: Finally, we have added manpage generation to the CMake builds as well. The new single source of truth solution can either use the sometimes more up-to-date version from the [documentation](https://syslog-ng.github.io/admin-guide/190_The_syslog-ng_manual_pages/README), or, by default, the locally available ready-to-use instance (which is also synced from the online version, but may lag behind). This can be controlled via the `--with-mapages=local|remote`, or the `-DWITH_MANPAGES=LOCAL|REMOTE` configuration options.
    ([#5459](https://github.com/syslog-ng/syslog-ng/pull/5459))


## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Alexander Zikulnig, Ben Ireland, Bálint Horváth, Gyula Kerekes,
Hofi, Romain Tartière, Ross Williams, Tamas Pal
