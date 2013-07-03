#! /bin/sh

set -e

TMPD="$(mktemp -d)"
TARGET="${@:-$(mktemp -d)}"

cp debian/changelog "${TMPD}/changelog.orig"

# Generate the orig.tar.xz
echo "* Generating the original tarball..."

debian/tools/get-orig-source.sh > "${TMPD}/get-orig-source.log"

ORIG=$(tail -n 1 "${TMPD}/get-orig-source.log" | sed -e "s,^.*available in ,,")

EXPORT_ROOT="${TMPD}/export"
install -d "${EXPORT_ROOT}"

export EXPORT_ROOT

debian/tools/bootstrap.sh
debian/tools/export-source.sh > "${TMPD}/export-source.log"

EROOT="$(tail -n 1 "${TMPD}/export-source.log" | sed -e "s,^.*available in ,,")"

cd "${EROOT}"

mv "${ORIG}" ..

# Make a normal dsc
debian/tools/bootstrap.sh
dpkg-buildpackage -us -uc -S -sa >/dev/null

git clean -fdx
cp "${TMPD}/changelog.orig" debian/changelog

# Make a systemd-enabled dsc
echo "* Generating the systemd-enabled package...."

dch -l.systemd --preserve "Custom build with systemd support enabled."
dch -r ""
printf "1\ns,systemd1,systemd,\nw\nq\n" | ed -s debian/changelog >/dev/null || true
debian/tools/bootstrap.sh systemd
dpkg-buildpackage -us -uc -S -sa >/dev/null

git clean -fdx
cp "${TMPD}/changelog.orig" debian/changelog

# Make a sysvinit dsc
echo "* Generate the systvinit package..."
dch -l.sysvinit --preserve "Custom build with systemd support disabled."
dch -r ""
printf "1\ns,sysvinit1,sysvinit,\nw\nq\n" | ed -s debian/changelog >/dev/null || true
debian/tools/bootstrap.sh ""
dpkg-buildpackage -us -uc -S -sa >/dev/null

## Copy the finished artifacts to their final place
cd ..

install -d "${TARGET}"
mv syslog-ng_* "${TARGET}/"

cd "${TARGET}"

rm -rf "${TMPD}"

echo
echo "Source packages are available at ${TARGET}/" | sed -e "s,.,=,g"
echo "Source packages are available at ${TARGET}/"
echo "Source packages are available at ${TARGET}/" | sed -e "s,.,=,g"
