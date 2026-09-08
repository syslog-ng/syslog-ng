#!/bin/bash
#############################################################################
# Copyright (c) 2026 Airbus Commercial Aircraft
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

# ------------------------------------------------------------------
# tarbackup – simple backup wrapper that tars + gzips a folder.
#
# Usage:
#   ./tarbackup.sh <folder-to-backup>
#
# The archive will be written to the current working directory with a
# name of the form:
#     YYYY-MM-DD_HHMM_<basename>.tar.gz
#
# Example:
#   $ ./tarbackup.sh ./myfolder
#   Backing up "./myfolder" ---> "2026-01-27_1721_myfolder.tar.gz"
# ------------------------------------------------------------------

set -euo pipefail # safer shell behaviour

# --------------------
# Helper functions
# --------------------

error() {
    echo >&2 "ERROR: $*"
}

info() {
    echo "$*"
}

# --------------------
# Argument validation
# --------------------
if [[ $# -ne 1 ]]; then
    error "Missing folder argument."
    echo "Usage: $0 <folder-to-backup>"
    exit 1
fi

SRC_DIR="$1"

# Resolve to an absolute path (helpful if the script is run from elsewhere)
ABS_SRC="$(realpath "$SRC_DIR")" || {
    error "Failed to resolve absolute path for '$SRC_DIR'."
    exit 1
}

# Check that it really exists and is a directory
if [[ ! -d $ABS_SRC ]]; then
    error "Directory does not exist: $ABS_SRC"
    exit 1
fi

# --------------------
# Build archive name
# --------------------
TIMESTAMP="$(date +%F_%H%M)"       # e.g. 2026-01-27_1721
BASE_NAME="$(basename "$ABS_SRC")" # e.g. myfolder
ARCHIVE="${TIMESTAMP}_${BASE_NAME}.tar.gz"

info "Backing up \"$ABS_SRC\" ---> \"$ARCHIVE\""

# --------------------
# Create the archive
# --------------------
# -C <dir> tells tar to change into that directory before archiving,
#   so we get a clean relative path in the tarball.
tar czvf "$ARCHIVE" -C "$(dirname "$ABS_SRC")" "$BASE_NAME"

info "Backup finished: $ARCHIVE"

# 7z a -tzip -p'topsecret' -mem=AES256 backup.zip ./myfolder/
