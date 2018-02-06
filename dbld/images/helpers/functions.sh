#!/bin/bash

# Helper functions for container creation.

function add_obs_repo_debian {
    DISTRO=$1
    wget -qO - http://download.opensuse.org/repositories/home:/laszlo_budai:/syslog-ng/${DISTRO}/Release.key | apt-key add -
    echo "deb http://download.opensuse.org/repositories/home:/laszlo_budai:/syslog-ng/"${DISTRO}" ./" | tee --append /etc/apt/sources.list.d/syslog-ng-obs.list
    apt-get update -qq
}

function add_czanik_repo_centos6 {
    cd /etc/yum.repos.d
    wget https://copr.fedorainfracloud.org/coprs/czanik/syslog-ng39epel6/repo/epel-6/czanik-syslog-ng39epel6-epel-6.repo
}

function add_czanik_repo_centos7 {
    cd /etc/yum.repos.d
    wget https://copr.fedorainfracloud.org/coprs/czanik/syslog-ng311/repo/epel-7/czanik-syslog-ng311-epel-7.repo
}


function step_down_from_root_with_gosu {
    ARCHITECTURE=$1
    cat /tmp/gosu.pubkey | gpg --import
    wget -O /usr/local/bin/gosu "https://github.com/tianon/gosu/releases/download/1.7/gosu-${ARCHITECTURE}"
    wget -O /usr/local/bin/gosu.asc "https://github.com/tianon/gosu/releases/download/1.7/gosu-${ARCHITECTURE}.asc"
    gpg --verify /usr/local/bin/gosu.asc
    rm /usr/local/bin/gosu.asc
    chmod +x /usr/local/bin/gosu
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

# DO NOT REMOVE!
"$@"
