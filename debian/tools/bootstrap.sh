#! /bin/sh

set -e

# Figure out the current version..
UPSTREAM_VERSION="$(dpkg-parsechangelog | sed -n '/Version:/s/Version: \(.*\)\.\(dfsg\|original\).*/\1/p' | sed -e 's,~\(.*\),,')"
UPSTREAM_MAJOR="$(echo ${UPSTREAM_VERSION} | cut -d . -f 1,2)"

git submodule --quiet update --init

for branch in upstream/mirror upstream/dfsg patched debian; do
	if ! git branch | grep -q "\s${branch}/${UPSTREAM_MAJOR}\$"; then
		git branch --track ${branch}/${UPSTREAM_MAJOR} origin/${branch}/${UPSTREAM_MAJOR}
	fi
done

debian/tools/update-control.sh ${UPSTREAM_VERSION}
