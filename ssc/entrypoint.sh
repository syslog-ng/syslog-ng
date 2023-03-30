#!/usr/bin/env bash

. /var/lib/syslog-ng/python-venv/bin/activate
exec /sbin/entrypoint.sh "$@"
