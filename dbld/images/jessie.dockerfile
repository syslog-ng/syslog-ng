FROM debian:jessie
MAINTAINER Andras Mitzki <andras.mitzki@balabit.com>, Laszlo Szemere <laszlo.szemere@balabit.com>

ADD helpers/functions.sh .

# Install packages
RUN apt-get update -qq && apt-get install --no-install-recommends -y \
  python-pip \
  python-setuptools \
  wget

ADD required-packages/jessie-pip.txt .
RUN cat jessie-pip.txt | grep -v "#" | xargs pip install 

ADD required-packages/jessie-dist.txt .
RUN cat jessie-dist.txt | grep -v "#" | xargs apt-get install --no-install-recommends -y

ADD required-packages/jessie-obs.txt .
RUN ./functions.sh add_obs_repo_debian Debian_8.0
RUN cat jessie-obs.txt | grep -v "#" | xargs apt-get install --no-install-recommends -y


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
