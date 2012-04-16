#! /bin/sh

set -e

# Figure out the current version..
UPSTREAM_VERSION="$(dpkg-parsechangelog | grep '^Version:' | sed -e 's,^Version: ,,' -e 's,\.\(dfsg\|original\),,' -e 's,-[0-9\.]*~\(.*\),,')"
UPSTREAM_MAJOR="$(echo ${UPSTREAM_VERSION} | cut -d . -f 1,2)"

git submodule --quiet update --init

for branch in upstream/mirror patched debian; do
	if ! git branch | grep -q "\s${branch}/${UPSTREAM_MAJOR}\$"; then
		git branch --track ${branch}/${UPSTREAM_MAJOR} origin/${branch}/${UPSTREAM_MAJOR}
	fi
done

debian/tools/update-control.sh ${UPSTREAM_VERSION}
