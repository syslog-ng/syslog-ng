# `balabit/syslog-ng-trusty`
This image provides a development environment to build and install syslog-ng from source. You have to clone the source
code of [syslog-ng ](https://github.com/balabit/syslog-ng.git) into a directory on your host machine then you can mount it
into the container (under `/source`).

## Building syslog-ng from source

Assume that we have cloned syslog-ng's source into the `$HOME/syslog-ng`
directory.  The following commands starts a container mounted with the
source:

```bash
$ dbld/rules shell-trusty
```

You can also build an RPM using:

```bash
$ dbld/rules deb-trusty
```

You can find the resulting debs in `$(top_srcdir)/dbld/build`.

You can also use this image to hack on syslog-ng by configuring and building
manually.

```bash
$ cd /source/
$ pip install -r requirements.txt
$ ./autogen.sh
$ mkdir build
$ cd build/
$ ../configure --enable-debug --prefix=/install
$ make
$ make check
$ make install
```

If the compilation and installation was successful you can run syslog-ng with the following command:

```bash
$ /install/syslog-ng/sbin/syslog-ng -Fedv
```

The source code and build products are mounted externally in a directory
called `/source` (for the sources) `/build` (for build products) and
`/install` (for the installed binaries) respectively.
