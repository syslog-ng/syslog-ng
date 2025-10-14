4.10.2
======

syslog-ng provides [RPM](https://github.com/syslog-ng/syslog-ng#rhel) and [DEB](https://github.com/syslog-ng/syslog-ng#debianubuntu) package repositories for Ubuntu, Debian, and RHEL, for both amd64 and arm64 architectures.

We also provide ready-to-test binaries in [Docker containers](https://hub.docker.com/r/balabit/syslog-ng/tags) based on the current stable Debian image.

For more details, visit our [Documentation Center](https://syslog-ng.github.io/)

Version 4.10.2 is a bugfix release, and resolves the following issues.

## Bugfixes

- Queued stats values of destinations could still contain weird values.\
  ([#5527](https://github.com/syslog-ng/syslog-ng/pull/5527))
- Unclosed standard file handlers could cause a hang when syslog-ng was started as a daemon.\
  ([#5532](https://github.com/syslog-ng/syslog-ng/pull/5532))
- The WebHook Python module could hang syslog-ng when started by systemd and `systemctl stop syslog-ng` was requested.\
  ([#5522](https://github.com/syslog-ng/syslog-ng/pull/5522))
- Our installer packages now include static index web pages and are browsable via [RPM](https://ose-repo.syslog-ng.com/yum/) and [DEB](https://ose-repo.syslog-ng.com/apt/).\
  ([#5517](https://github.com/syslog-ng/syslog-ng/pull/5517), [#5518](https://github.com/syslog-ng/syslog-ng/pull/5518), [#5520](https://github.com/syslog-ng/syslog-ng/pull/5520))

## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Vivek Anand, Hofi, @gotyaoi, Kovacs Gergo Ferenc, Tamas Pal, @Tinkiter
