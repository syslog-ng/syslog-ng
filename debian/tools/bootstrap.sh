#! /bin/sh

set -e

# Figure out the current version..
UPSTREAM_VERSION="$(dpkg-parsechangelog | grep '^Version:' | sed -e 's,^Version: ,,' -e 's,\.\(dfsg\|original\),,' -e 's,-[0-9\.]*~\(.*\),,')"
UPSTREAM_MAJOR="$(echo ${UPSTREAM_VERSION} | cut -d . -f 1,2)"

git submodule --quiet update --recursive --init

debian/tools/update-control.sh ${UPSTREAM_VERSION}
