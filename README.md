[![Build Status](https://github.com/syslog-ng/syslog-ng/actions/workflows/devshell.yml/badge.svg)](https://github.com/syslog-ng/syslog-ng/actions/workflows/devshell.yml)
[![Nightly](https://github.com/syslog-ng/syslog-ng/actions/workflows/nightly-release.yml/badge.svg)](https://github.com/syslog-ng/syslog-ng/actions/workflows/nightly-release.yml)
[![Binary packages](https://github.com/syslog-ng/syslog-ng/actions/workflows/packages.yml/badge.svg)](https://github.com/syslog-ng/syslog-ng/actions/workflows/packages.yml)
[![Compile dbld-images](https://github.com/syslog-ng/syslog-ng/actions/workflows/dbld-images.yml/badge.svg)](https://github.com/syslog-ng/syslog-ng/actions/workflows/dbld-images.yml)

syslog-ng
=========

syslog-ng is an enhanced log daemon, supporting a wide range of input
and output methods: syslog, unstructured text, message queues,
databases (SQL and NoSQL alike), and more.

## Quickstart

The simplest configuration accepts system logs from /dev/log (from
applications or forwarded by systemd) and writes everything to a single
file:

``` config
@version: 4.8
@include "scl.conf"

log {
	source { system(); };
	destination { file("/var/log/syslog"); };
};
```

This one additionally processes logs from the network (TCP/514 by default):

``` config
@version: 4.8
@include "scl.conf"

log {
	source {
		system();
		network();
	};
	destination { file("/var/log/syslog"); };
};
```
This config is designed for structured/application logging, using local submission via JSON, and outputting in key=value format:

``` config
@version: 4.8
@include "scl.conf"

log {
	source { system(); };
	destination { file("/var/log/app.log" template("$(format-welf --subkeys .cim.)\n")); };
};
```

To submit a structured log using `logger`, you might run:

```shell
$ logger '@cim: {"name1":"value1", "name2":"value2"}'
```

In which case the resulting message will be:

``` text
name1=value1 name2=value2
```

For a brief introduction to configuring the syslog-ng application, see the [quickstart guide](https://syslog-ng.github.io/admin-guide/040_Quick-start_guide/README).

## Features

* Receive and send [RFC3164](https://tools.ietf.org/html/rfc3164)
  and [RFC5424](https://tools.ietf.org/html/rfc5424) style syslog
  messages
* Receive and send [JSON](http://json.org/) formatted messages
* Work with any kind of unstructured data
* Classify and structure logs using built-in parsers (csv-parser(),
  db-parser(), kv-parser(), etc.)
* Normalize, crunch, and process logs as they flow through the system
* Hand over logs for further processing using files, message queues (like
  [AMQP](http://www.amqp.org/)), or databases (like
  [PostgreSQL](http://www.postgresql.org/) or
  [MongoDB](http://www.mongodb.org/))
* Forward logs to big data tools (like [Elasticsearch](https://www.elastic.co/),
  [Apache Kafka](http://kafka.apache.org/), or
  [Apache Hadoop](http://hadoop.apache.org/))

### Performance

* syslog-ng provides performance levels comparable to a large
  cluster when running on a single node
* In the simplest use case, it scales up to 600-800k messages per
  second
* But classification, parsing, and filtering still produce several
  tens of thousands of messages per second

### Community

* syslog-ng is developed by a community of volunteers, the best way to
  contact us is via our [github project page](http://github.com/syslog-ng/syslog-ng)
  project, our [gitter channel](https://gitter.im/syslog-ng/syslog-ng) or
  our [mailing list](https://lists.balabit.hu/mailman/listinfo/syslog-ng).
* syslog-ng is integrated into almost all Linux distributions and BSDs, it
  is also incorporated into a number of products, see our [powered by
  syslog-ng](https://syslog-ng.com/powered-by-syslog-ng) page for more details.

### Sponsors

- [Balabit](http://www.balabit.com/) is the original commercial sponsor of the syslog-ng project, and was acquired by One Identity in 2018. One Identity offers a commercial edition for syslog-ng, called the syslog-ng Premium Edition.
- Axoflow is the company of Balazs Scheidler, the original creator and main developer of syslog-ng.

## Feedback

We are really interested to see who uses our software, so if you do use it and you like
what you see, please tell us about it. A star on github or an email
saying thanks means a lot already, but telling us about your use case,
your experience, and things to improve would be much appreciated.

Just send an email to feedback (at) syslog-ng.org.

 *Feedback Powers Open Source.*

## Installation from source

Releases and precompiled tarballs are available on [GitHub][github-repo].

 [github-repo]: https://github.com/syslog-ng/syslog-ng/releases

To compile from source, the easiest is to use `dbld`, a docker based,
self-hosted compile/build/release infrastructure within the source tree. See
`dbld/README.md` for more information.

For the brave souls who want to compile syslog-ng from scratch, the usual
drill applies:

    $ ./configure && make && make install

The extra effort in contrast with the dbld based build is the need to fetch
and install all build dependencies of syslog-ng (of which there are a few).

If you don't have a configure script (because of cloning from git, for example),
run `./autogen.sh` to generate it.

Some of the functionality of syslog-ng is compiled only if the required
development libraries are present. The configure script displays a
summary of enabled features at the end of its run.
For details, see the [syslog-ng compiling instructions](https://syslog-ng.github.io/admin-guide/030_Installing_syslog-ng/000_Compiling_syslog-ng_from_source).

## Installation from binaries

Binaries are available in various Linux distributions and contributors
maintain packages of the latest and greatest syslog-ng version for
various OSes.

### Debian/Ubuntu

Simply invoke the following command as root:

    # apt install syslog-ng

The latest versions of syslog-ng are available for a wide range of Debian
and Ubuntu releases from our APT repository.

The packages and the APT repository are provided "as is" without warranty of any kind, on a best-effort level.

#### Supported distributions

syslog-ng packages are released for the following distribution versions (x86-64):

| Distro version | sources.list component name |
|---|---|
| Ubuntu 24.04 | ubuntu-noble |
| Ubuntu 22.04 | ubuntu-jammy |
| Ubuntu 20.04 | ubuntu-focal |
| Debian 12 | debian-bookworm |
| Debian 11 | debian-bullseye |
| Debian Unstable | debian-sid |
| Debian Testing | debian-testing |

#### Adding the APT repository

1. Download and install the release signing key:

    ``` shell
    wget -qO - https://ose-repo.syslog-ng.com/apt/syslog-ng-ose-pub.asc | sudo apt-key add -
    ```

2. Add the repository containing the latest build of syslog-ng to the APT sources. For example, stable releases on Ubuntu 22.04:

    ``` shell
    echo "deb https://ose-repo.syslog-ng.com/apt/ stable ubuntu-jammy" | sudo tee -a /etc/apt/sources.list.d/syslog-ng-ose.list
    ```

3. Run `apt update`

#### Nightly builds

Nightly packages are built and released from the git `master` branch everyday.

Use `nightly` instead of `stable` in step 2 to use the nightly APT repository. E.g.:

``` shell
echo "deb https://ose-repo.syslog-ng.com/apt/ nightly ubuntu-noble" | sudo tee -a /etc/apt/sources.list.d/syslog-ng-ose.list
```

Nightly builds can be used for testing purposes (obtaining new features and bugfixes) at the risk of breakage.

### Arch Linux

``` shell
# pacman -S syslog-ng
```

### Fedora

syslog-ng is available as a Fedora package that you can install using
dnf:

#### dnf install syslog-ng

You can download packages for the latest versions from [here](https://copr.fedoraproject.org/coprs/czanik/).

For instructions on how to install syslog-ng on RPM distributions, see the blog post [Installing latest syslog-ng on RHEL and other RPM distributions](https://syslog-ng.com/blog/installing-latest-syslog-ng-on-rhel-and-other-rpm-distributions/).

If you wish to install the latest RPM package that comes from a recent commit in Git for testing purposes, read the blog post, [RPM packages from syslog-ng Git HEAD](https://syslog-ng.com/blog/rpm-packages-from-syslog-ng-git-head/).

### macOS

``` shell
# brew install syslog-ng
```

### Others

Binaries for other platforms are listed on the
official [third party page][3rd-party].

 [3rd-party]: https://syslog-ng.com/3rd-party-binaries

## Installation from Docker image

Binaries are also available as a Docker image. To find out more, check out the blog post, [Your central log server in Docker](https://syslog-ng.com/blog/central-log-server-docker/).

## Documentation

For the latest, markdown based version, see the [syslog-ng documentation](https://syslog-ng.github.io) center. \
The official documentation of the earlier versions (3.X) of syslog-ng Open Source Edition provided by One Identity is available
[here](https://support.oneidentity.com/syslog-ng-open-source-edition/).

## Contributing

If you would like to contribute to syslog-ng, to fix a bug or create a new module, the [syslog-ng pages](https://syslog-ng.github.io/dev-guide/README) helps you take the first steps to working with the code base.
