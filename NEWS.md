4.10.1
======

syslog-ng provides [RPM](https://github.com/syslog-ng/syslog-ng#rhel) and [DEB](https://github.com/syslog-ng/syslog-ng#debianubuntu) package repositories for Ubuntu, Debian, and RHEL, for both amd64 and arm64 architectures.

We also provide ready-to-test binaries in [Docker containers](https://hub.docker.com/r/balabit/syslog-ng/tags) based on the current stable Debian image.

For more details, visit our [Documentation Center](https://syslog-ng.github.io/)

## Highlights

Version 4.10.1 is a bugfix release, not needed by most users. It fixes the syslog-ng container and platform support in some less common situations.

## Bugfixes

- You can now compile syslog-ng on FreeBSD 15 again.\
  ([#5506](https://github.com/syslog-ng/syslog-ng/pull/5506))
- The syslog-ng container has Python support working again.\
  ([#5488](https://github.com/syslog-ng/syslog-ng/pull/5488))
- Stackdump support compiles only on glibc Linux systems, but 	it also used to be enabled on others when libunwind was present. 	This problem affected embedded Linux systems using alternative libc 	implementations like OpenWRT and Gentoo in some cases. It is now 	turned off by default, therefore it needs to be explicitly enabled.\
  ([#5506](https://github.com/syslog-ng/syslog-ng/pull/5506))

## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Hofi, Kovacs Gergo Ferenc, Bruno Marinier, Josef Schlehofer
