#!/usr/bin/env bash
#############################################################################
# Copyright (c) 2026 One Identity LLC.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
# As an additional exemption you are allowed to compile & link against the
# OpenSSL libraries as published by the OpenSSL project. See the file
# COPYING for details.
#
#############################################################################
#
# compare-builds.sh — configure, build, install, and compare autotools vs cmake
#
# Uses the canonical OSE build commands verbatim (adapted for separate dirs).
#
# Usage:
#   ./dev-utils/compare-builds.sh [--configure-only] [--jobs N]
#
# --configure-only   Configure both, print summaries side-by-side, then exit.
# --jobs N           Parallel jobs (default: nproc).
#
# Output directories (relative to repo root):
#   build.at-cmp/        autotools build tree
#   build.at-cmp/install autotools install
#   build.cm-cmp/        cmake build tree
#   build.cm-cmp/install cmake install

set -euo pipefail

REPO=$(cd "$(dirname "$0")/.." && pwd)
AT_BUILD="$REPO/build.at-cmp"
CM_BUILD="$REPO/build.cm-cmp"
AT_INSTALL="$AT_BUILD/install"
CM_INSTALL="$CM_BUILD/install"

CONFIGURE_ONLY=0
JOBS=$(nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 4)

while [[ $# -gt 0 ]]; do
  case $1 in
    --configure-only) CONFIGURE_ONLY=1; shift ;;
    --jobs) JOBS="$2"; shift 2 ;;
    --jobs=*) JOBS="${1#--jobs=}"; shift ;;
    *) echo "Unknown arg: $1"; exit 1 ;;
  esac
done

# ---------------------------------------------------------------------------
# Phase 1: configure both
# ---------------------------------------------------------------------------
echo "======================================================="
echo "Phase 1: configure"
echo "======================================================="

# --- autotools ---
echo ""
echo "--- autotools configure ---"
rm -rf "$AT_BUILD" "$CM_BUILD"
mkdir -p "$AT_BUILD" "$CM_BUILD"
# Always regenerate configure — it's a generated file not kept in the source tree
(cd "$REPO" && ./autogen.sh 2>&1) | tee "$AT_BUILD/autogen.log" >/dev/null
(
  cd "$AT_BUILD"
  CFLAGS="-DUSE_IV_FOR_TIME_INVALIDATION=0 -DNO_PTHREAD_SELF_USAGE_WARNING=1 ${CFLAGS:-}" \
  "$REPO/configure" \
    --prefix="$AT_INSTALL" \
    --enable-debug \
    --with-ivykis=internal \
    --enable-all-modules \
    --enable-slog \
    --enable-pacct \
    --enable-jemalloc \
    --enable-ebpf \
    --disable-java \
    --disable-java-modules \
    --with-python=auto \
    --with-python-packages=venv \
    --enable-manpages \
    --enable-manpages-install \
    --enable-tests \
    --enable-colored-log \
    --enable-extra-warnings \
    --enable-werror
) 2>&1 | tee "$AT_BUILD/configure.log" >/dev/null

# --- cmake ---
echo "--- cmake configure ---"
(
  cd "$REPO"
  CFLAGS="-DUSE_IV_FOR_TIME_INVALIDATION=0 -DNO_PTHREAD_SELF_USAGE_WARNING=1 -Werror ${CFLAGS:-}" \
  cmake \
    --install-prefix "$CM_INSTALL" \
    -B "$CM_BUILD" \
    -S . \
    -Wno-dev \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=OFF \
    -DCMAKE_BUILD_TYPE=Debug \
    -DIVYKIS_SOURCE=internal \
    -DENABLE_JAVA=OFF \
    -DENABLE_JAVA_MODULES=OFF \
    -DENABLE_CPP=ON \
    -DENABLE_GRPC=ON \
    -DENABLE_SLOG=ON \
    -DENABLE_EBPF=ON \
    -DENABLE_SPOOF_SOURCE=ON \
    -DPYTHON_VERSION=AUTO \
    -DPYTHON_PACKAGES_INSTALL=venv \
    -DENABLE_MANPAGES=ON \
    -DENABLE_MANPAGES_INSTALL=ON \
    -DBUILD_TESTING=ON \
    -DCMAKE_VERBOSE_MAKEFILE=OFF \
    -DENABLE_FORCE_GNU99=ON \
    -DSUMMARY_LEVEL=0 \
    -DENABLE_EXTRA_WARNINGS=ON \
    -DENABLE_WERROR=ON
) 2>&1 | tee "$CM_BUILD/cmake-configure.log" >/dev/null

# --- summaries ---
echo ""
echo "======================================================="
echo "autotools summary:"
echo "======================================================="
sed -n '/^-----------/,$ p' "$AT_BUILD/configure.log"

echo ""
echo "======================================================="
echo "cmake summary:"
echo "======================================================="
sed -n '/^-----------/,$ p' "$CM_BUILD/cmake-configure.log"

if [[ $CONFIGURE_ONLY -eq 1 ]]; then
  echo ""
  echo "=== --configure-only: review summaries above before running the full build ==="
  exit 0
fi

# ---------------------------------------------------------------------------
# Phase 2: build + install
# ---------------------------------------------------------------------------
echo ""
echo "======================================================="
echo "Phase 2: build + install (jobs=$JOBS)"
echo "======================================================="

echo "--- autotools build ---"
make AM_DEFAULT_VERBOSITY=0 -C "$AT_BUILD" install -j"$JOBS"

echo ""
echo "--- cmake build+install ---"
(
  cd "$REPO"
  CFLAGS="-DUSE_IV_FOR_TIME_INVALIDATION=0 -DNO_PTHREAD_SELF_USAGE_WARNING=1 -Werror ${CFLAGS:-}" \
  cmake --build "$CM_BUILD" --target install -j"$JOBS"
)

# ---------------------------------------------------------------------------
# Phase 2.5: strip known build-system-structural differences before comparing
# ---------------------------------------------------------------------------
# .la files are libtool metadata produced only by autotools — cmake never generates them.
find "$AT_INSTALL" -name '*.la' -delete

# ---------------------------------------------------------------------------
# Phase 3: install tree comparison
# ---------------------------------------------------------------------------
echo ""
echo "======================================================="
echo "Phase 3: install tree comparison"
echo "  AT: $AT_INSTALL"
echo "  CM: $CM_INSTALL"
echo "======================================================="

cmp_dir() {
  local dir="$1"
  local a c at_only cm_only
  a=$(find "$AT_INSTALL/$dir" -maxdepth 1 -printf '%f\n' 2>/dev/null | grep -v "^${dir##*/}$" | grep -v '^$' | sort || true)
  c=$(find "$CM_INSTALL/$dir" -maxdepth 1 -printf '%f\n' 2>/dev/null | grep -v "^${dir##*/}$" | grep -v '^$' | sort || true)
  at_only=$(comm -23 <(echo "$a") <(echo "$c") | grep -v '^$' || true)
  cm_only=$(comm -13 <(echo "$a") <(echo "$c") | grep -v '^$' || true)
  if [[ -n "$at_only" || -n "$cm_only" ]]; then
    echo ""
    echo "### $dir/"
    [[ -n "$at_only" ]] && echo "  AT only:" && echo "$at_only" | sed 's/^/    /' || true
    [[ -n "$cm_only" ]] && echo "  CM only:" && echo "$cm_only" | sed 's/^/    /' || true
    DIFF_FOUND=$((DIFF_FOUND + 1))
  fi
}

DIFF_FOUND=0
for d in "lib" "lib/syslog-ng" "lib/syslog-ng/loggen" "lib/syslog-ng/libtest" \
         "lib/pkgconfig" "include/syslog-ng" "include/syslog-ng/libtest" "bin" "sbin"; do
  cmp_dir "$d"
done

echo ""
if [[ $DIFF_FOUND -eq 0 ]]; then
  echo "=== comparison done: IDENTICAL ==="
else
  echo "=== comparison done: $DIFF_FOUND DIFFERENCE(S) FOUND ==="
  exit 1
fi
