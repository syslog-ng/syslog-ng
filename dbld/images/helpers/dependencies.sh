#!/bin/bash

set -e
set -x

function install_criterion() {
    CRITERION_VERSION=2.3.2
    download_target "https://github.com/Snaipe/Criterion/releases/download/v${CRITERION_VERSION}/criterion-v${CRITERION_VERSION}-linux-x86_64.tar.bz2" /tmp/criterion.tar.bz2
    tar xvjf /tmp/criterion.tar.bz2 --strip 1 -C /usr
    rm -rf /tmp/criterion.tar.bz2
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

function install_apt_packages {
    apt-get update -qq -o Acquire::CompressionTypes::Order::=gz
    filter_packages_by_platform /helpers/packages.manifest | xargs apt-get install --no-install-recommends --yes
}

function install_yum_packages {
    yum install -y epel-release
    filter_packages_by_platform /helpers/packages.manifest | xargs yum install -y
}

function install_pip_packages {
    if [ "${OS_PLATFORM}" == "centos-6" ]; then
        filter_packages_by_platform /helpers/pip_packages.manifest | xargs pip install -U
    else
        python -m pip install --upgrade pip
        filter_packages_by_platform /helpers/pip_packages.manifest | xargs python -m pip install -U
    fi
}

function enable_dbgsyms {
    echo "deb http://ddebs.ubuntu.com $(lsb_release -cs) main restricted universe multiverse
deb http://ddebs.ubuntu.com $(lsb_release -cs)-updates main restricted universe multiverse
deb http://ddebs.ubuntu.com $(lsb_release -cs)-proposed main restricted universe multiverse" | \
    tee -a /etc/apt/sources.list.d/ddebs.list
    apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys 428D7C01 C8CAB6595FDFF622
}

function install_perf {
    apt-cache search linux-tools | grep 'linux-tools-.*-generic' | cut -d" " -f1 | tail -n1 | cut -d"-" -f1-4 | xargs apt-get install --no-install-recommends -y
}


# DO NOT REMOVE!
"$@"
