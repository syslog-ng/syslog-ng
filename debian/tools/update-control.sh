#! /bin/sh

set -e

if [ "$#" -lt 1 ]; then
	cat <<EOF
Usage: $0 UPSTREAM_VERSION
EOF
	exit 1
fi

UPSTREAM_VERSION="$1"
UPSTREAM_VERSION_MAJOR="$(echo "$1" | cut -d "." -f 1,2)"

# Update debian/control
sed -e "s,@UPSTREAM_VERSION@,${UPSTREAM_VERSION},g" \
    -e "s,@UPSTREAM_VERSION_MAJOR@,${UPSTREAM_VERSION_MAJOR},g" \
    < debian/control.d/control.in \
    > debian/control
