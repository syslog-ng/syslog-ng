#! /bin/sh

set -e

EXPORT_ROOT="${EXPORT_ROOT:-../deb-packages}"

# =========================================== #

DSRC=$(dpkg-parsechangelog | sed -n '/^Source:/s/Source: //p')
DUMAJOR=$(dpkg-parsechangelog | sed -n '/^Version:/s/Version: //p' | cut -d. -f1,2)
DVERSION=$(dpkg-parsechangelog | sed -n '/^Version:/s/Version: //p')
EXPORT_ID="${@:-packaging/debian/${DUMAJOR}}"
WD=$(pwd)
TDIR="${EXPORT_ROOT}/${DSRC}"
EXPORT_DIR="${TDIR}/${DSRC}-${DUMAJOR}"

##
# Boilerplate
##

echo "* Exporting ${DSRC}-${DUMAJOR} (${DVERSION}) from ${EXPORT_ID} to ${TDIR}"

install -d "${TDIR}"
cd "${TDIR}"

##
# Clone the debian packaging to a temp dir, and extract the submodule
# commit id.
##

echo "** Cloning: Debian packaging..."

TMPD="$(mktemp -d --tmpdir sng-deb-pack.XXXXXXXX)"

git clone -q "${WD}" "${TMPD}"

cd "${TMPD}"
git checkout -q "${EXPORT_ID}" >/dev/null

UCID="$(git submodule status syslog-ng-${DUMAJOR} | cut -d" " -f1 | tr -d '-')"

cd "${WD}"
rm -rf "${TMPD}"

##
# Clone the patched repo
##

echo "** Cloning: Patched upstream sources..."

git clone -q "${WD}" "${EXPORT_DIR}"
cd "${EXPORT_DIR}"
git checkout -q -b "packaging/debian/export/${DUMAJOR}" "${UCID}" >/dev/null

##
# Merge the patched repo with the debian/ stuff
##

echo "** Merging: Patched sources & debian packaging..."

cp .gitmodules .gitmodules-backup
git merge -q --no-commit -s recursive -X theirs "${EXPORT_ID}"
cat .gitmodules-backup >>.gitmodules
rm -f .gitmodules-backup
git submodule sync
git add .gitmodules
rm -rf "syslog-ng-${DUMAJOR}"
git rm -qf debian/control debian/changelog.in debian/README.Debian \
        debian/syslog-ng.conf.example \
        debian/syslog-ng.default debian/syslog-ng.docs \
        debian/syslog-ng.files debian/syslog-ng.init \
        debian/syslog-ng.logrotate debian/syslog-ng.logrotate.example \
        debian/syslog-ng.postinst debian/syslog-ng.postrm \
        debian/syslog-ng.preinst \
        .travis.yml
git rm -q syslog-ng-"${DUMAJOR}"

EDITOR=cat git commit -q >/dev/null

cd "${WD}"
echo "* Exported tree available in ${TDIR}/syslog-ng-${DUMAJOR}"
