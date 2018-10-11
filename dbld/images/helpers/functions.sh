#!/bin/bash

set -e

# Helper functions for container creation.

function enable_dbgsyms {
    apt-get install --no-install-recommends -y lsb-release dirmngr
    echo "deb http://ddebs.ubuntu.com $(lsb_release -cs) main restricted universe multiverse
deb http://ddebs.ubuntu.com $(lsb_release -cs)-updates main restricted universe multiverse
deb http://ddebs.ubuntu.com $(lsb_release -cs)-proposed main restricted universe multiverse" | \
    tee -a /etc/apt/sources.list.d/ddebs.list
    apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys 428D7C01 C8CAB6595FDFF622
    apt-get update
}

function install_perf {
    apt-cache search linux-tools | grep 'linux-tools-.*-generic' | cut -d" " -f1 | tail -n1 | cut -d"-" -f1-4 | xargs apt-get install --no-install-recommends -y
}

# DO NOT REMOVE!
"$@"
