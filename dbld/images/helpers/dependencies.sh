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

function install_libmaxminddb() {
    LIBMAXMINDDB_VERSION=1.3.2
    download_target "https://github.com/maxmind/libmaxminddb/releases/download/${LIBMAXMINDDB_VERSION}/libmaxminddb-${LIBMAXMINDDB_VERSION}.tar.gz" /tmp/libmaxminddb.tar.gz
    cd /tmp/
    tar xvf /tmp/libmaxminddb.tar.gz
    cd /tmp/libmaxminddb-${LIBMAXMINDDB_VERSION}
    ./configure
    make
    make install
    ldconfig
    rm -rf /tmp/libmaxminddb.tar.gz /tmp/libmaxminddb-${LIBMAXMINDDB_VERSION}
}

function install_mongoc() {
    MONGOC_VERSION=1.13.0
    download_target "https://github.com/mongodb/mongo-c-driver/releases/download/${MONGOC_VERSION}/mongo-c-driver-${MONGOC_VERSION}.tar.gz" /tmp/mongo-c-driver.tar.gz
    cd /tmp/
    tar xvf /tmp/mongo-c-driver.tar.gz
    cd /tmp/mongo-c-driver-${MONGOC_VERSION}
    mkdir cmake-build/
    cd cmake-build/
    cmake -DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF ..
    make
    make install
    rm -rf /tmp/mongo-c-driver.tar.gz /tmp/mongo-c-driver-${MONGOC_VERSION}
}

function install_rabbitmqc() {
    RABBITMQC_VERSION=0.9.0
    download_target "https://github.com/alanxz/rabbitmq-c/archive/v${RABBITMQC_VERSION}.tar.gz" /tmp/rabbitmq-c.tar.gz
    cd /tmp/
    tar xfz /tmp/rabbitmq-c.tar.gz
    cd /tmp/rabbitmq-c-${RABBITMQC_VERSION}/
    mkdir cmake-build/
    cd cmake-build/
    cmake ..
    make
    make install
    rm -rf /tmp/rabbitmq-c.tar.gz /tmp/rabbitmq-c-${RABBITMQC_VERSION}/
}

function install_riemann() {
    RIEMANN_VERSION=1.10.3
    download_target "https://github.com/algernon/riemann-c-client/archive/riemann-c-client-${RIEMANN_VERSION}.tar.gz" /tmp/riemann.tar.gz
    cd /tmp/
    tar xfz /tmp/riemann.tar.gz
    cd /tmp/riemann-c-client-riemann-c-client-${RIEMANN_VERSION}
    autoreconf -i
    ./configure
    make
    make install
    rm -rf /tmp/riemann.tar.gz /tmp/riemann-c-client-riemann-c-client-${RIEMANN_VERSION}
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
