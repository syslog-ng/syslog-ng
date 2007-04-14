#!/bin/sh
RUNPROTOTYPE="../prototype"
find opt/ -exec chown root:bin \{\} \;
strip opt/syslog-ng/sbin/syslog-ng
echo "i pkginfo" > $RUNPROTOTYPE
echo "i admin" >> $RUNPROTOTYPE
echo "i space" >> $RUNPROTOTYPE
# no dependency, at least for now
#echo "i depend" >> $RUNPROTOTYPE
echo "d none opt/syslog-ng 0755 root bin" >> $RUNPROTOTYPE
pkgproto -c application opt/syslog-ng/sbin >> $RUNPROTOTYPE
pkgproto -c application opt/syslog-ng/etc >> $RUNPROTOTYPE
pkgproto -c manpages opt/syslog-ng/man >> $RUNPROTOTYPE
pkgproto -c none opt/syslog-ng/doc >> $RUNPROTOTYPE

