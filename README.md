syslog-ng
=========

syslog-ng is an enhanced log daemon, supporting a wide range of input
and output methods: syslog, unstructured text, message queues,
databases (SQL and NoSQL alike) and more.

Key features:
  * receive and send [RFC3164](https://tools.ietf.org/html/rfc3164)
    and [RFC5424](https://tools.ietf.org/html/rfc5424) style syslog
    messages
  * work with any kind of unstructured data
  * receive and send [JSON](http://json.org/) formatted messages
  * classify and structure logs with builtin parsers (csv-parser(),
    db-parser(), ...)
  * normalize, crunch and process logs as they flow through the system
  * hand on messages for further processing using message queues (like
    [AMQP](http://www.amqp.org/)), files or databases (like
    [PostgreSQL](http://www.postgresql.org/) or
    [MongoDB](http://www.mongodb.org/)).

Performance:
  * syslog-ng provides performance levels comparable to a large
    cluster while running on a single node.
  * In the simplest use-case it scales up 600-800k messages per
    second.
  * But classification, parsing and filtering still produces several
    tens of thousands messages per second.

Installation from Source
========================

Releases are tagged in the [github repository][github-repo] and
tarballs ready to compile are made available at [BalaBit][balabit]'s
syslog-ng [tarball repository][balabit-download].

 [github-repo]: https://github.com/balabit/syslog-ng/releases
 [balabit]: http://www.balabit.com/
 [balabit-download]: http://www.balabit.com/network-security/syslog-ng/opensource-logging-system/downloads/download/syslog-ng-ose/3.5

To compile from source, the usuall drill applies (assuming you have
the required dependencies):

    $ ./configure && make && make install

Some of the functionality is compiled only in case the required
development libraries are present. The configure script displays a
summary of enabled features at the end of its run.


Installation from Binaries
==========================

Binaries are available in various Linux distributions and contributors
maintain packages of the latest and greatest syslog-ng version for
various OSes.

Debian/Ubuntu
-------------

Simply invoke the following command as root:

    # apt-get install syslog-ng

Latest versions of syslog-ng are available for a wide range of Debian
and Ubuntu releases and architectures from an
[unofficial repository][madhouse-repo].

 [madhouse-repo]: http://asylum.madhouse-project.org/projects/debian/

Fedora
------

syslog-ng is available as a Fedora package that you can install using
yum:

    # yum install syslog-ng

Others
------

Binaries for other platforms might be available, please check out the
official [third party page][3rd-party] for more information.

 [3rd-party]: http://www.balabit.com/network-security/syslog-ng/opensource-logging-system/downloads/3rd-party
