#!/bin/bash

set -e

# Helper functions for container creation.

function add_obs_repo {
    DISTRO=$1
    wget -qO - http://download.opensuse.org/repositories/home:/laszlo_budai:/syslog-ng/${DISTRO}/Release.key | apt-key add -
    echo "deb http://download.opensuse.org/repositories/home:/laszlo_budai:/syslog-ng/"${DISTRO}" ./" | tee --append /etc/apt/sources.list.d/syslog-ng-obs.list
    apt-get update -qq
}

function add_epel_repo {
    cd /etc/yum.repos.d
    if [ "$1" == "centos6" ]; then
        wget https://copr.fedorainfracloud.org/coprs/czanik/syslog-ng39epel6/repo/epel-6/czanik-syslog-ng39epel6-epel-6.repo
    elif [ "$1" == "centos7" ]; then
        wget https://copr.fedorainfracloud.org/coprs/czanik/syslog-ng-githead/repo/epel-7/czanik-syslog-ng-githead-epel-7.repo
    else
        return 1
    fi
}

function gradle_installer {
    export GRADLE_HOME=/opt/gradle
    export GRADLE_VERSION=4.1
    wget --no-verbose --output-document=gradle.zip "https://services.gradle.org/distributions/gradle-${GRADLE_VERSION}-bin.zip"

    unzip gradle.zip
    rm gradle.zip
    mv "gradle-${GRADLE_VERSION}" "${GRADLE_HOME}/"
    ln --symbolic "${GRADLE_HOME}/bin/gradle" /usr/bin/gradle
}

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
