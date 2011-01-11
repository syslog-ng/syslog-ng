#!/bin/bash
#
# syslog-ng.sh, postinstall script for Cygwin
#
# This file is part of the Cygwin port of syslog-ng.

# Directory where the config files are stored
SYSCONFDIR="/etc/syslog-ng"

# Old config file, up to 3.0.1.
OLDSYSCONFFILE="/etc/syslog-ng.conf"

# Paths of the config files starting with 3.2.1.
SYSCONFFILE="${SYSCONFDIR}/syslog-ng.conf"
SCLFILE="${SYSCONFDIR}/scl.conf"
MODULESFILE="${SYSCONFDIR}/modules.conf"

# Directory in which the templates are stored.
DEFAULTDIR="/etc/defaults"

# Directory to keep state information
LOCALSTATEDIR=/var/lib/syslog-ng

# If an old syslog-ng.conf file exists, but no new one, move the old
# one over to the new configuration directory.
if [ -f "${OLDSYSCONFFILE}" -a ! -f "${SYSCONFFILE}" ]
then
  # Check for ${SYSCONFDIR} directory, create it if necessary.
  [ -e "${SYSCONFDIR}" -a ! -d "${SYSCONFDIR}" ] && exit 1
  [ ! -e "${SYSCONFDIR}" ] && mkdir -p "${SYSCONFDIR}"
  [ ! -e "${SYSCONFDIR}" ] && exit 2
  setfacl -m u:system:rwx "${SYSCONFDIR}"
  mv -f "${OLDSYSCONFFILE}" "${SYSCONFFILE}"
  setfacl -m u:system:rw- "${SYSCONFFILE}"
  # Always copy template files to ${SYSCONFDIR} and add a ".new" suffix
  # to the ${SYSCONFFILE} template so the admin knows something has changed.
  cp "${DEFAULTDIR}${SYSCONFFILE}" "${SYSCONFFILE}.new"
  cp "${DEFAULTDIR}${SCLFILE}" "${SCLFILE}"
  cp "${DEFAULTDIR}${MODULESFILE}" "${MODULESFILE}"
fi

# Make sure /var/lib/syslog-ng exists.
if [ -e "${LOCALSTATEDIR}" -a ! -d "${LOCALSTATEDIR}" ]
then
  rm -f "${LOCALSTATEDIR}"
  [ -e "${LOCALSTATEDIR}" ] && exit 3
fi
[ ! -e "${LOCALSTATEDIR}" ] && mkdir -p "${LOCALSTATEDIR}"
[ ! -d "${LOCALSTATEDIR}" ] && exit 4
setfacl -m u:system:rwx "${LOCALSTATEDIR}"
exit 0
