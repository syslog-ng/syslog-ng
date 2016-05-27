#!/bin/sh -x
#############################################################################
# Copyright (c) 2016 Balabit
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as published
# by the Free Software Foundation, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
# As an additional exemption you are allowed to compile & link against the
# OpenSSL libraries as published by the OpenSSL project. See the file
# COPYING for details.
#
#############################################################################

build_dbi() {
  build_dbi_core || exit 1
  build_dbd || exit 1
}

build_dbi_core() {
  if ! pkg-config --exists "dbi >= 0.9.0"; then
    OPWD="$PWD"
    mkdir -p "$SLNG_CACHE/deps" || exit 1
    cd "$SLNG_CACHE/deps" || exit 1

    if [ ! -d "libdbi" ]; then
      git clone --quiet --branch libdbi-0.9.0 \
        git://git.code.sf.net/p/libdbi/libdbi || exit 1
    fi

    cd libdbi || exit 1
    if [ ! -f configure ]; then
      ./autogen.sh || exit 1
    fi;

    if [ ! -f Makefile ]; then
      ./configure --prefix="$SLNG" \
        --disable-docs --disable-dependency-tracking || exit 1
    fi

    make -j install-exec || exit 1
    cd "$OPWD"
  fi
}

build_dbd() {
  if [ ! -d "$SLNG/lib/dbd" ]; then
    local OPWD="$PWD"
    mkdir -p "$SLNG_CACHE/deps" || exit 1
    cd "$SLNG_CACHE/deps" || exit 1

    if [ ! -d "libdbi-drivers" ]; then
      git clone --quiet --branch libdbi-drivers-0.9.0 \
        git://git.code.sf.net/p/libdbi-drivers/libdbi-drivers || exit 1
    fi

    cd libdbi-drivers || exit 1
    if [ ! -f configure ]; then
      sed -i "s~^ac_dbi_libdir=\"no\"$~\# & \# syslog-ng HACK~" configure.in || exit 1
      ./autogen.sh || exit 1
    fi

    if [ ! -f Makefile ]; then
      local DBIINC="`pkg-config --variable=includedir dbi`" || exit 1
      [ -n "$DBIINC" ] || exit 1
      local DBILIB="`pkg-config --variable=libdir dbi`" || exit 1
      [ -n "$DBILIB" ] || exit 1

      ./configure --prefix="$SLNG" --with-sqlite3 \
                --with-dbi-libdir="$DBILIB" \
                --with-dbi-incdir="$DBIINC/.." \
                --disable-docs --disable-dependency-tracking || exit 1
    fi

    make -j install-exec || exit 1
    cd "$OPWD"
  fi
}

build_dbi "$@"
