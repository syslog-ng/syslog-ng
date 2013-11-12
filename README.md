syslog-ng
=========

syslog-ng is an enhanced log daemon, supporting a wide range of input and output 
methods: syslog, unstructured text, queueing, SQL & NoSQL.

Key features:
  * receive and send RFC3164 and RFC5424 style syslog messages
  * work with any kind of unstructured data, 
  * receive and send JSON formatted messages
  * classify and structure logs with builtin parsers (csv-parser(), db-parser() ...)
  * normalize, crunch and process logs as they flow through the system
  * hand on messages for further processing using queueing, files, SQL, MongoDB

Performance:
  * syslog-ng provides performance levels comparable to a large cluster while 
    running on a single node. 
  * In the simplest use-case it scales up 600-800k messages per second. 
  * But classification, parsing and filtering still produces several tens of 
    thousands messages per second.

Installation from Binaries
==========================

Binaries are available in various Linux distributions and we contributors maintain packages of
the latest and greatest syslog-ng version for various OSes.

Debian/Ubuntu
-------------

Simply invoke the following command as root:

  # apt-get install syslog-ng
  
The madhouse repositories make the latest versions of syslog-ng available for a wide range of 
Debian/Ubuntu releases, architectures, here's a description on how to use them:

http://asylum.madhouse-project.org/projects/debian/

Fedora
------

syslog-ng is available as a Fedora package that you can install using yum:

  # yum install syslog-ng


Others
------

Binaries for other platforms might be available, please check out this page that describes them:

http://www.balabit.com/network-security/syslog-ng/opensource-logging-system/downloads/3rd-party

Installation from Source
========================

Releases are tagged in the github repository and tarballs ready to compile are made available at this location:

http://www.balabit.com/network-security/syslog-ng/opensource-logging-system/downloads/download/syslog-ng-ose/3.5

To compile from source, the usuall drill applies (assuming you have the required dependencies):

  $ ./configure; make; make install
  
Some of the functionality is compiled only in case the required development libraries are present. The configure
script displays a summary of enabled features at the end of its run.

