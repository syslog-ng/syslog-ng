#! /bin/sh

set -e

# Figure out the current version..
UPSTREAM_VERSION="$(dpkg-parsechangelog | grep '^Version:' | sed -e 's,^Version: ,,' -e 's,\.\(dfsg\|original\),,' -e 's,-[0-9\.]*[~\+]\(.*\),,')"
UPSTREAM_MAJOR="$(echo ${UPSTREAM_VERSION} | cut -d . -f 1,2)"

git submodule --quiet update --init
if [ -d syslog-ng-${UPSTREAM_MAJOR} ]; then
        (cd syslog-ng-${UPSTREAM_MAJOR} && git submodule --quiet sync)
        (cd syslog-ng-${UPSTREAM_MAJOR} && echo " [@$(git rev-parse --short HEAD)]" >../debian/source_revision)
else
        git submodule --quiet sync
fi
git submodule --quiet update --recursive --init

if [ "$(dpkg-vendor --query Vendor)" = "Debian" ]; then
        features="systemd"
else
        features=""
fi

features="$features $@"

debian/tools/update-control.sh ${UPSTREAM_VERSION} ${features}
