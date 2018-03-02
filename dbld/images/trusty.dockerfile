FROM ubuntu:14.04
MAINTAINER Andras Mitzki <andras.mitzki@balabit.com>, Laszlo Szemere <laszlo.szemere@balabit.com>

ADD helpers/functions.sh .

# Install packages
RUN apt-get update -qq && apt-get install --no-install-recommends -y \
  python-pip \
  python-setuptools \
  wget

ADD required-pip/all.txt pip-all.txt
RUN cat pip-*.txt | grep -v '^$\|^#' | xargs pip install

ADD required-apt/all.txt apt-all.txt
ADD required-apt/trusty.txt apt-trusty.txt
RUN cat apt-*.txt | grep -v '^$\|^#' | xargs apt-get install --no-install-recommends -y

RUN ./functions.sh add_obs_repo xUbuntu_14.04
ADD required-obs/all.txt obs-all.txt
RUN cat obs-*.txt | grep -v '^$\|^#' | xargs apt-get install --no-install-recommends -y


# grab gosu for easy step-down from root
ADD helpers/gosu.pubkey /tmp
RUN ./functions.sh step_down_from_root_with_gosu $(dpkg --print-architecture)


# mount points for source code
RUN mkdir /source
VOLUME /source
VOLUME /build


ADD helpers/entrypoint-debian.sh /
ENTRYPOINT ["/entrypoint-debian.sh"]
