FROM ubuntu:17.10
MAINTAINER Andras Mitzki <andras.mitzki@balabit.com>, Laszlo Szemere <laszlo.szemere@balabit.com>

ADD helpers/functions.sh .

# Install packages
RUN apt-get update -qq && apt-get install --no-install-recommends -y \
  python-pip \
  python-setuptools \
  wget

ADD required-pip/all.txt pip-all.txt
RUN cat pip-*.txt | grep -v '^$\|^#'| xargs pip install

ADD required-apt/all.txt apt-all.txt
ADD required-apt/artful.txt apt-artful.txt
RUN cat apt-*.txt | grep -v '^$\|^#' | xargs apt-get install --no-install-recommends -y

RUN ./functions.sh add_obs_repo xUbuntu_17.04
ADD required-obs/all.txt obs-all.txt
RUN cat obs-*.txt | grep -v '^$\|^#' | xargs apt-get install --no-install-recommends -y


RUN cd /tmp && wget http://ftp.de.debian.org/debian/pool/main/libn/libnative-platform-java/libnative-platform-jni_0.11-5_$(dpkg --print-architecture).deb
RUN cd /tmp && dpkg -i libnative-platform-jni*.deb


# grab gosu for easy step-down from root
ADD helpers/gosu.pubkey /tmp
RUN ./functions.sh step_down_from_root_with_gosu $(dpkg --print-architecture)


# add a fake sudo command
ADD helpers/fake-sudo.sh /usr/bin/sudo


# mount points for source code
RUN mkdir /source
VOLUME /source
VOLUME /build


ADD helpers/entrypoint-debian.sh /
ENTRYPOINT ["/entrypoint-debian.sh"]
