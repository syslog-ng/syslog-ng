#! /bin/sh

while ! test -f config.status || test "$(pwd)" = "/"; do
        cd ..
done

pwd
