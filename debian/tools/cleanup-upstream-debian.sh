#! /bin/sh
set -e

rm -f README.markdown

cd debian

rm -f \
   Makefile \
   README.Debian \
   changelog.in \
   syslog-ng.conf.example \
   syslog-ng.default \
   syslog-ng.files \
   syslog-ng.init \
   syslog-ng.logrotate \
   syslog-ng.logrotate.example \
   syslog-ng.postinst \
   syslog-ng.postrm \
   syslog-ng.preinst

touch -r ../Makefile.am Makefile.am
touch -r ../Makefile.in Makefile.in
