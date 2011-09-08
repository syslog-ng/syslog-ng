#! /bin/sh

set -e

##
# Find out where the top of the tree is, and do some sanity checking.
##

SELF="$(readlink -e "$0")"
TOP="$(readlink -e "$(dirname "${SELF}")/../../")"

cd "${TOP}"

if [ ! -d .git ]; then
	echo "get-orig-source: This script must be somewhere under a git repository!" >&2
	exit 1
fi

UPSTREAM_VERSION=$(git tag -l 'dfsg/*' 2>/dev/null | sort | tail -n 1 | sed -e 's,^dfsg/,,')

if [ -z "${UPSTREAM_VERSION}" ]; then
	echo "get-orig-source: Cannot determine the upstream version!" >&2
	exit 1
fi

git submodule update --init
install -d debian/orig-source

##
# Assemble the full source
##

# syslog-ng
git archive "dfsg/${UPSTREAM_VERSION}" --format tar --prefix=syslog-ng/ | \
	tar -C debian/orig-source -xf -

# embedded ivykis
(
	cd lib/ivykis
	git archive HEAD --format tar --prefix=syslog-ng/lib/ivykis/
) | tar -C debian/orig-source -xf -

# embedded libmongo-client
(
	cd modules/afmongodb/libmongo-client
	git archive HEAD --format tar --prefix=syslog-ng/modules/afmongodb/libmongo-client/
) | tar -C debian/orig-source -xf -

##
# Create the orig.tar.xz from the assembled sources
##
tar -C debian/orig-source -cf - syslog-ng | \
	xz -9 >syslog-ng_${UPSTREAM_VERSION}.orig.tar.xz

rm -rf debian/orig-source
