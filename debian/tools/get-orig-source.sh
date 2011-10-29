#! /bin/sh

set -e

WD=$(pwd)
TDIR=$(mktemp -d --tmpdir sng-upstream.XXXXXXXX)
cd "${TDIR}"

echo "* Building syslog-ng in ${TDIR}"

##
# Check out the git sources
##

echo "** Cloning..."

git clone -q git://git.madhouse-project.org/debian/syslog-ng.git
cd syslog-ng
git submodule --quiet update --init

UPSTREAM_TAG=$(git tag -l 'dfsg/*' 2>/dev/null | sort | tail -n 1)
UPSTREAM_VERSION=$(echo "${UPSTREAM_TAG}" | sed -e 's,^dfsg/,,' -e "s,\.release,," | tr '_' '~')

if [ -z "${UPSTREAM_VERSION}" ]; then
	echo "get-orig-source: Cannot determine the upstream version!" >&2
	exit 1
fi

install -d debian/orig-source

##
# Assemble the full source
##

echo "** Exporting..."

# syslog-ng
git archive "${UPSTREAM_TAG}" --format tar --prefix=syslog-ng/ | \
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

echo "** Creating the upstream tarball..."

tar -C debian/orig-source -cf - syslog-ng | \
	xz -9 >"${WD}"/syslog-ng_${UPSTREAM_VERSION}.dfsg.orig.tar.xz

##
# Cleanup
##
echo "** Cleaning up."

cd "${WD}"
rm -rf "${TDIR}"

echo "* Upstream tarball available in ${WD}/syslog-ng_${UPSTREAM_VERSION}.dfsg.orig.tar.xz"
