#!/bin/bash

local_setup()
{
  echo "LANGUAGE=en_US.UTF-8" > /etc/environment
  echo "LC_ALL=en_US.UTF-8" >> /etc/environment
}

host_setup()
{
  echo "syslog-ng.devenv" > /etc/hostname
}

ubuntu_update()
{
  sudo apt-get update -qq -y
  sudo apt-get install -qq -y curl
  sudo curl --silent http://packages.madhouse-project.org/debian/add-release.sh | sudo sh
  sudo apt-get update -qq -y
}

ubuntu_install()
{
  sudo apt-get install -qq -y pkg-config flex bison xsltproc docbook-xsl libevtlog-dev libnet1-dev libglib2.0-dev libdbi0-dev libssl-dev libjson0-dev libwrap0-dev libpcre3-dev libcap-dev libesmtp-dev libgeoip-dev libhiredis-dev sqlite3 libdbd-sqlite3 libriemann-client-dev
  sudo apt-get install -qq -y git vim autoconf automake libtool zsh
}

devenv_setup()
{
  cd /vagrant
  ./autogen.sh
  ./configure --with-ivykis=internal           \
              --prefix=/home/vagrant/install/  \
              --enable-pacct 
  make
  make install
  cd /home/vagrant
  sudo ln -s /vagrant /home/vagrant/project
}

# Bootstrap
local_setup
host_setup
ubuntu_update
ubuntu_install
devenv_setup
