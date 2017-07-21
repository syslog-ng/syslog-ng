#!/bin/sh
LIBPROTOTYPE="../prototype.lib"
find usr/local/lib -exec chown root:bin \{\} \;
find usr/local/lib  -type f -name *.so* -exec strip \{\} \;

echo "i pkginfo" > $LIBPROTOTYPE
echo "i admin" >> $LIBPROTOTYPE
echo "d none opt 0755 root bin" >> $LIBPROTOTYPE
echo "d none usr/local 0755 root bin" >> $LIBPROTOTYPE
echo "d none usr/local/lib 0755 root bin" >> $LIBPROTOTYPE
# /usr/bin/grep is a f*ing LAME program !!!
pkgproto -c library usr/local/lib | grep -v "usr/local/lib/pkgconfig/eventlog.pc" | grep -v "usr/local/lib/libevtlog.a" | grep -v "usr/local/lib/libevtlog.la" >> $LIBPROTOTYPE

DEVPROTOTYPE="../prototype.dev"
echo "i pkginfo" > $DEVPROTOTYPE
echo "i admin" >> $DEVPROTOTYPE
echo "d none opt 0755 root bin" >> $DEVPROTOTYPE
echo "d none usr/local 0755 root bin" >> $DEVPROTOTYPE
echo "d none usr/local/lib 0755 root bin" >> $DEVPROTOTYPE
echo "d none usr/local/lib/pkgconfig 0755 root bin" >> $DEVPROTOTYPE
pkgproto -c headers usr/local/include >> $DEVPROTOTYPE
pkgproto -c headers usr/local/lib/pkgconfig/eventlog.pc >> $DEVPROTOTYPE
pkgproto -c headers usr/local/lib/libevtlog.a >> $DEVPROTOTYPE
pkgproto -c headers usr/local/lib/libevtlog.la >> $DEVPROTOTYPE

