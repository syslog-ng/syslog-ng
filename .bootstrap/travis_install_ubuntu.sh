#!/bin/bash

sudo apt-get update -qq -y
sudo apt-get install -qq -y curl
curl --silent http://packages.madhouse-project.org/debian/add-release.sh | sudo sh
sudo apt-get update -qq -y
sudo apt-get install -qq -y pkg-config flex bison xsltproc docbook-xsl libevtlog-dev libnet1-dev libglib2.0-dev libdbi0-dev libssl-dev libjson0-dev libwrap0-dev libpcre3-dev libcap-dev libesmtp-dev libgeoip-dev libhiredis-dev sqlite3 libdbd-sqlite3 libriemann-client-dev
sudo pip install nose ply pep8 pylint


