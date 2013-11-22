#!/bin/sh

set -e

lcov --capture --directory `pwd` --compat-libtool --output-file coverage.info --base-directory "${top_srcdir}"
lcov --extract coverage.info "${top_srcdir}/*" --output-file coverage.info
lcov --remove coverage.info "${top_srcdir}/modules/afmongodb/libmongo-client/*" --output-file coverage.info
lcov --remove coverage.info "${top_srcdir}/modules/afamqp/rabbitmq-c/*" --output-file coverage.info
genhtml coverage.info --output-directory coverage.html
