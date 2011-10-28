#! /bin/sh

DEFAULT_UPSTREAM="3.3"

set -e

if [ "$#" -lt 1 ]; then
	cat <<EOF
Usage: $0 UPSTREAM_VERSION
EOF
	exit 1
fi

UPSTREAM_VERSION="$1"
UPSTREAM_VERSION_MAJOR="$(echo "$1" | cut -d "." -f 1,2)"
if [ "${DEFAULT_UPSTREAM}" = "${UPSTREAM_VERSION_MAJOR}" ]; then
	EXTRA_VERSION=""
else
	EXTRA_VERSION="-${UPSTREAM_VERSION_MAJOR}"
fi

# Update debian/control
sed -e "s,@UPSTREAM_VERSION@,${UPSTREAM_VERSION},g" \
    -e "s,@UPSTREAM_VERSION_MAJOR@,${UPSTREAM_VERSION_MAJOR},g" \
    -e "s,@EXTRA_VERSION@,${EXTRA_VERSION},g" \
    < debian/control.d/control.in \
    > debian/control

# Remove old libsyslog-ng-${VERSION}.* files
rm -f debian/libsyslog-ng-[0-9].*.install \
      debian/libsyslog-ng-[0-9].*.lintian-overrides

# Update debian/libsyslog-ng-${UPSTREAM_VERSION}.install
cp debian/control.d/libsyslog-ng.install \
   debian/libsyslog-ng-${UPSTREAM_VERSION}.install

# Update debian/libsyslog-ng-${UPSTREAM_VERSION}.lintian-overrides
sed -e "s,@UPSTREAM_VERSION@,${UPSTREAM_VERSION},g" \
    -e "s,@UPSTREAM_VERSION_MAJOR@,${UPSTREAM_VERSION_MAJOR},g" \
    < debian/control.d/libsyslog-ng.lintian-overrides \
    > debian/libsyslog-ng-${UPSTREAM_VERSION}.lintian-overrides
