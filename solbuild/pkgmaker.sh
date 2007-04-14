#!/bin/sh
. ../pkginfo
. ../rules.conf
FILENAME=${RUNPKGNAME}_${VERSION}_${ARCH}.pkg
pkgmk -o -r `pwd` -f ../prototype -d spool
pkgtrans -nos spool $FILENAME $PKG
mv -f /var/spool/pkg/$FILENAME ../../../
gzip --best -f ../../../$FILENAME
rm -rf /var/spool/pkg/$PKG

