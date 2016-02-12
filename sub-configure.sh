#!/bin/sh

# comes from a substitution in autogen.sh
CONFIGURE_OPTS="@__CONFIGURE_OPTS__@"

configure="`dirname $0`/`basename $0 .gnu`"
echo "Running: " $configure $@ $CONFIGURE_OPTS
$SHELL $configure "$@" $CONFIGURE_OPTS
