#!/bin/bash -x

sudo apt-get build-dep syslog-ng

function create_syslog_ng_ose_dirs() {
    current_version=$1
    ose_branch=`echo ${current_version} | cut -d"_" -f4`
    mkdir -p ~/syslog_ng_ose/${current_version}/source
    cd ~/syslog_ng_ose/${current_version}/source
    if [ ${ose_branch} = 3.7 ] ; then
        git clone -b master https://github.com/balabit/syslog-ng.git .
    else
        git clone -b ${ose_branch}/master https://github.com/balabit/syslog-ng.git .
    fi
}

create_syslog_ng_ose_dirs syslog_ng_ose_3.5
create_syslog_ng_ose_dirs syslog_ng_ose_3.6
create_syslog_ng_ose_dirs syslog_ng_ose_3.7

function update_syslog_ng_ose_projects() {
    git fetch --all && git reset --hard origin/master
}

function configure_make_make_install() {
    current_version=$1
    mkdir -p ~/syslog_ng_ose/${current_version}/install_dir
    cd ~/syslog_ng_ose/${current_version}/install_dir
    install_dir=`pwd`
    cd ~/syslog_ng_ose/${current_version}/source
    ./autogen.sh && ./configure --prefix=${install_dir} --with-ivykis=internal && make && sudo make install
    sudo chown -R `whoami`:`whoami` ${install_dir}
}

configure_make_make_install syslog_ng_ose_3.5
configure_make_make_install syslog_ng_ose_3.6
configure_make_make_install syslog_ng_ose_3.7
