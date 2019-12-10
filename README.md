[![Gitter](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/syslog-ng/syslog-ng?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=body_badge)
[![Build Status](https://travis-ci.org/syslog-ng/syslog-ng.svg?branch=master)](https://travis-ci.org/syslog-ng/syslog-ng)

syslog-ng
=========

syslog-ng is an enhanced log daemon, supporting a wide range of input
and output methods: syslog, unstructured text, message queues,
databases (SQL and NoSQL alike), and more.

## Quickstart

The simplest configuration accepts system logs from /dev/log (from
applications or forwarded by systemd) and writes everything to a single
file:

```
@version: 3.25
@include "scl.conf"

log {
	source { system(); };
	destination { file("/var/log/syslog"); };
};
```

This one additionally processes logs from the network (TCP/514 by default):

```
@version: 3.25
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

```
@version: 3.25
@include "scl.conf"

log {
	source { system(); };
	destination { file("/var/log/app.log" template("$(format-welf --subkeys .cim.)\n")); };
};
```

To submit a structured log using `logger`, you might run:

```
$ logger '@cim: {"name1":"value1", "name2":"value2"}'
```

In which case the resulting message will be:

```
name1=value1 name2=value2
```
For a brief introduction to configuring the syslog-ng application, see the [quickstart guide](https://support.oneidentity.com/technical-documents/syslog-ng-open-source-edition/administration-guide/the-syslog-ng-ose-quick-start-guide).


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

[Balabit](http://www.balabit.com/) is the original creator and largest current
sponsor of the syslog-ng project. They offer support,
professional services, and addons you may be interested in

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

To compile from source, the usual drill applies (assuming you have
the required dependencies):

    $ ./configure && make && make install

If you don't have a configure script (because of cloning from git, for example),
run `./autogen.sh` to generate it.

Some of the functionality of syslog-ng is compiled only if the required
development libraries are present. The configure script displays a
summary of enabled features at the end of its run.
For details, see the [syslog-ng compiling instructions](https://support.oneidentity.com/technical-documents/syslog-ng-open-source-edition/administration-guide/installing-syslog-ng).


## Installation from binaries

Binaries are available in various Linux distributions and contributors
maintain packages of the latest and greatest syslog-ng version for
various OSes.

### Debian/Ubuntu

Simply invoke the following command as root:

    # apt-get install syslog-ng

The latest versions of syslog-ng are available for a wide range of Debian
and Ubuntu releases and architectures from an
[unofficial repository](https://build.opensuse.org/project/show/home:laszlo_budai:syslog-ng).

 [madhouse-repo]: http://asylum.madhouse-project.org/projects/debian/

For instructions on how to install syslog-ng on Debian/Ubuntu distributions, see the blog post [Installing the latest syslog-ng on Ubuntu and other DEB distributions](https://syslog-ng.com/blog/installing-the-latest-syslog-ng-on-ubuntu-and-other-deb-distributions/).

### Fedora

syslog-ng is available as a Fedora package that you can install using
yum:

    # yum install syslog-ng

You can download packages for the latest versions from [here](https://copr.fedoraproject.org/coprs/czanik/).

For instructions on how to install syslog-ng on RPM distributions, see the blog post [Installing latest syslog-ng on RHEL and other RPM distributions](https://syslog-ng.com/blog/installing-latest-syslog-ng-on-rhel-and-other-rpm-distributions/).

If you wish to install the latest RPM package that comes from a recent commit in Git for testing purposes, read the blog post, [RPM packages from syslog-ng Git HEAD](https://syslog-ng.com/blog/rpm-packages-from-syslog-ng-git-head/).

### Others

Binaries for other platforms are listed on the
official [third party page][3rd-party].

 [3rd-party]: https://syslog-ng.com/3rd-party-binaries

## Installation from Docker image

Binaries are also available as a Docker image. To find out more, check out the blog post, [Your central log server in Docker](https://syslog-ng.com/blog/central-log-server-docker/).

## Documentation

The documentation of the latest released version of syslog-ng Open Source Edition is available [here](https://www.syslog-ng.com/technical-documents/doc/syslog-ng-open-source-edition/3.25/administration-guide). For earlier versions, see the syslog-ng [Documentation Page](https://www.syslog-ng.com/technical-documents).

## Contributing

If you would like to contribute to syslog-ng, to fix a bug or create a new module, the [syslog-ng gitbook](https://syslog-ng.gitbooks.io/getting-started/content/) helps you take the first steps to working with the code base.
