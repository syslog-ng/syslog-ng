#!/bin/sh

cp -f ../pkginfo.lib ../pkginfo
. ../pkginfo
. ../rules.conf
LIBFILENAME=${LIBPKGNAME}_${VERSION}_${SOLBUILD_ARCH}.pkg
pkgmk -o -r `pwd` -f ../prototype.lib -d spool
pkgtrans -nos spool $LIBFILENAME $PKG
mv -f /var/spool/pkg/$LIBFILENAME ../../../
gzip --best -f ../../../$LIBFILENAME
rm -rf /var/spool/pkg/$LIBPKGNAME

cp -f ../pkginfo.dev ../pkginfo
. ../pkginfo
. ../rules.conf
DEVFILENAME=${DEVPKGNAME}_${VERSION}_${SOLBUILD_ARCH}.pkg
pkgmk -o -r `pwd` -f ../prototype.dev -d spool
pkgtrans -nos spool $DEVFILENAME $PKG
mv -f /var/spool/pkg/$DEVFILENAME ../../../
gzip -f --best ../../../$DEVFILENAME
rm -rf /var/spool/pkg/$DEVPKGNAME
