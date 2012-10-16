#! /bin/sh

set -e

UPSTREAM_VERSION="$(dpkg-parsechangelog | sed -n '/^Version:/{s,^Version: ,,p}' | cut -d- -f1 | sed -e "s,\.dfsg,,")"
case "${UPSTREAM_VERSION}" in
        *dfsg*)
                UPSTREAM_TAG="dfsg/$(echo "${UPSTREAM_VERSION}" | tr '~' '_')"
                ;;
        *original*)
                UPSTREAM_TAG="v$(echo "${UPSTREAM_VERSION}" | tr '~' '_' | sed -e 's,\.original,,')"
                ;;
        *sandbox*)
                UPSTREAM_MAJOR="$(echo "${UPSTREAM_VERSION}" | cut -d . -f 1,2)"
                UPSTREAM_TAG="sandbox/${UPSTREAM_MAJOR}"
                ;;
        *)
                UPSTREAM_TAG="v$(echo "${UPSTREAM_VERSION}" | tr '~' '_')"
                ;;
esac

WD=$(pwd)
TDIR=$(mktemp -d --tmpdir sng-upstream.XXXXXXXX)
cd "${TDIR}"

echo "* Building syslog-ng ${UPSTREAM_VERSION} from ${UPSTREAM_TAG} in ${TDIR}"

##
# Check out the git sources
##

echo "** Cloning..."

git clone -q git://git.madhouse-project.org/debian/syslog-ng.git
cd syslog-ng
git checkout -q "${UPSTREAM_TAG}"
git submodule --quiet update --init --recursive

install -d debian/orig-source

##
# Assemble the full source
##

echo "** Exporting..."

# syslog-ng
git archive "${UPSTREAM_TAG}" --format tar --prefix=syslog-ng/ | \
	tar -C debian/orig-source -xf -

embed_submodule ()
{
        submod="$1"
        
        (
                cd ${submod}
                git archive HEAD --format tar --prefix=syslog-ng/${submod}/
        ) | tar -C debian/orig-source -xf -
}

# embedded ivykis
for submod in $(git submodule status --recursive | cut -d " " -f 3); do
        echo "*** Embedding $submod..."
        embed_submodule $submod
done

##
# Create the orig.tar.xz from the assembled sources
##

echo "** Creating the upstream tarball..."

tar -C debian/orig-source -cf - syslog-ng | \
	xz -9 >"${WD}"/syslog-ng_${UPSTREAM_VERSION}.orig.tar.xz

##
# Cleanup
##
echo "** Cleaning up."

cd "${WD}"
rm -rf "${TDIR}"

echo "* Upstream tarball available in ${WD}/syslog-ng_${UPSTREAM_VERSION}.orig.tar.xz"
