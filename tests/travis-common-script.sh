#!/bin/bash
set -e

function main() {
    . tests/build-log-cflags-propagation.sh;
    if [ "$CC" = "gcc" ]; then
      export DISTCHECK_CONFIGURE_FLAGS="$CONFIGURE_FLAGS";
      exec_prop_check "make distcheck -j 3 V=1 --keep-going";
      find . -name test-suite.log | xargs cat;
    else
      make --keep-going -j $(nproc);
      S=$?;
      if [ "$S" = "0" ]; then
        make install
        . scripts/get-libjvm-path.sh || return $?;
        export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$JNI_LIBDIR";
        make func-test V=1;
        make pytest-self-check;
        make pytest-check;
      elif [ "$S" = "42" ]; then
        return $S;
      else
        make V=1 install;
        return $S;
      fi;
    fi
}

main
