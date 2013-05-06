#! /bin/sh
set -e

top_srcdir=$(cd ${top_srcdir}; pwd)
top_builddir=$(cd ${top_builddir}; pwd)
srcdir="${top_srcdir}/tests/functional"

export top_srcdir
export top_builddir
export srcdir

install -d ${top_builddir}/tests/functional
cd ${top_builddir}/tests/functional
${top_srcdir}/tests/functional/func_test.py
