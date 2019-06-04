#!/bin/bash

set -e
set -x

function install_cmake() {
    CMAKE_VERSION=3.12.2
    CMAKE_SHORT_VERSION=$(echo ${CMAKE_VERSION} | cut -d"." -f1-2)
    download_target "https://cmake.org/files/v${CMAKE_SHORT_VERSION}/cmake-${CMAKE_VERSION}-Linux-x86_64.sh" /tmp/cmake.sh
    chmod +x /tmp/cmake.sh
    mkdir -p /opt/cmake
    /tmp/cmake.sh --skip-license --prefix=/opt/cmake/
    ln -s /opt/cmake/bin/cmake /usr/bin/cmake
    rm -rf /tmp/cmake.sh
}

function install_criterion() {
    CRITERION_VERSION=2.3.3

    download_target "https://github.com/Snaipe/Criterion/releases/download/v${CRITERION_VERSION}/criterion-v${CRITERION_VERSION}.tar.bz2" /tmp/criterion.tar.bz2
    cd /tmp/
    tar xvf /tmp/criterion.tar.bz2
    cd /tmp/criterion-v${CRITERION_VERSION}
    cmake -DCMAKE_INSTALL_PREFIX=/usr .
    make install
    ldconfig
    rm -rf /tmp/criterion.tar.bz2 /tmp/criterion-v${CRITERION_VERSION}

}

function install_gosu() {
    GOSU_VERSION=1.10
    ARCHITECTURE=$1
    cat /helpers/gosu.pubkey | gpg --import
    download_target "https://github.com/tianon/gosu/releases/download/${GOSU_VERSION}/gosu-${ARCHITECTURE}" /usr/local/bin/gosu
    download_target "https://github.com/tianon/gosu/releases/download/${GOSU_VERSION}/gosu-${ARCHITECTURE}.asc" /usr/local/bin/gosu.asc
    gpg --verify /usr/local/bin/gosu.asc
    rm /usr/local/bin/gosu.asc
    chmod +x /usr/local/bin/gosu
}

function install_gradle {
    GRADLE_VERSION=4.10
    download_target "https://services.gradle.org/distributions/gradle-${GRADLE_VERSION}-bin.zip" /tmp/gradle.zip
    mkdir -p /opt/gradle
    unzip -d /opt/gradle /tmp/gradle.zip
    rm -rf /tmp/gradle.zip
    ln -s /opt/gradle/gradle-${GRADLE_VERSION}/bin/gradle /usr/bin/gradle
    find / -name 'libjvm.so' | sed 's@/libjvm.so@@g' | tee --append /etc/ld.so.conf.d/openjdk-libjvm.conf
    ldconfig
}

function download_target() {
    target=$1
    output=$2
    wget --no-check-certificate $target --output-document=$output
}

function filter_packages_by_platform {
    FILENAME=$1
    OS_GROUP=$(echo ${OS_PLATFORM} | cut -d"-" -f1)
    grep -v "#" ${FILENAME} | grep -e "${OS_PLATFORM}" -e "${OS_GROUP}[^-]" | cut -d"[" -f1
}

function add_obs_repo {
    apt-get update && apt-get install --no-install-recommends --yes wget gnupg2
    echo 'deb http://download.opensuse.org/repositories/home:/laszlo_budai:/syslog-ng/xUbuntu_18.04 ./' | tee /etc/apt/sources.list.d/lbudai.list
    cat | tee /etc/apt/preferences.d/lbudai <<EOF
Package: *
Pin: origin "download.opensuse.org"
Pin-Priority: 1
EOF
    wget -qO - http://download.opensuse.org/repositories/home:/laszlo_budai:/syslog-ng/xUbuntu_18.04/Release.key | apt-key add -
    apt-get update
}

function add_copr_repo {

    # NOTE: we are removing dnf/yum plugins after enabling copr as they
    # install a couple of Python dependencies which then conflict with our
    # PIP installation later.
    case "${OS_PLATFORM}" in
        centos-*)
            yum install -y yum-plugin-copr
            yum copr enable -y czanik/syslog-ng-githead
            ;;
        fedora-*)
            dnf install -y dnf-plugins-core
            dnf copr enable -y czanik/syslog-ng-githead
            ;;
    esac
}

function add_epel_repo {
    yum install -y epel-release
}

function install_apt_packages {
    apt-get update -qq -o Acquire::CompressionTypes::Order::=gz
    filter_packages_by_platform /helpers/packages.manifest | xargs -t apt-get install --no-install-recommends --yes
}

function install_yum_packages {
    yum install -y findutils
    filter_packages_by_platform /helpers/packages.manifest | xargs yum install -y
}

function install_pip_packages {
    case "${OS_PLATFORM}" in
        centos-7)
            python -m pip install --upgrade pip==9.0.3
            ;;
        *)
            python -m pip install --upgrade pip
            ;;
    esac
    filter_packages_by_platform /helpers/pip_packages.manifest | xargs python -m pip install -U
}

function enable_dbgsyms {
    echo "deb http://ddebs.ubuntu.com $(lsb_release -cs) main restricted universe multiverse" | tee -a /etc/apt/sources.list.d/ddebs.list
    echo "deb http://ddebs.ubuntu.com $(lsb_release -cs)-updates main restricted universe multiverse" | tee -a /etc/apt/sources.list.d/ddebs.list
    apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys 428D7C01 C8CAB6595FDFF622
}

function install_perf {
    apt-cache search linux-tools | grep 'linux-tools-.*-generic' | cut -d" " -f1 | tail -n1 | cut -d"-" -f1-4 | xargs apt-get install --no-install-recommends -y
}


# DO NOT REMOVE!
"$@"
