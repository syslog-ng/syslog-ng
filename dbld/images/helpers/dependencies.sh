#!/bin/bash

set -e
set -x

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

function download_target() {
    target=$1
    output=$2
    wget --no-check-certificate $target --output-document=$output
}

# DO NOT REMOVE!
"$@"
