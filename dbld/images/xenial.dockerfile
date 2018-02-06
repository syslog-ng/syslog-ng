FROM ubuntu:16.04
MAINTAINER Andras Mitzki <andras.mitzki@balabit.com>, Laszlo Szemere <laszlo.szemere@balabit.com>

ADD helpers/functions.sh .

# Install packages
RUN apt-get update -qq && apt-get install --no-install-recommends -y \
  python-pip \
  python-setuptools \
  wget

ADD required-packages/xenial-pip.txt .
RUN cat xenial-pip.txt | grep -v "#" | xargs pip install --upgrade pip

ADD required-packages/xenial-dist.txt .
RUN cat xenial-dist.txt | grep -v "#" | xargs apt-get install --no-install-recommends -y

ADD required-packages/xenial-obs.txt .
RUN ./functions.sh add_obs_repo_debian xUbuntu_16.04
RUN cat xenial-obs.txt | grep -v "#" | xargs apt-get install --no-install-recommends -y


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
