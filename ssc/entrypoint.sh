#!/usr/bin/env bash

. /var/lib/syslog-ng-venv/bin/activate
exec /sbin/entrypoint.sh "$@"
