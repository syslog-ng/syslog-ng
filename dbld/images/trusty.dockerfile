FROM ubuntu:14.04
MAINTAINER Andras Mitzki <andras.mitzki@balabit.com>, Laszlo Szemere <laszlo.szemere@balabit.com>

ADD helpers/functions.sh .

# Install packages
RUN apt-get update -qq && apt-get install --no-install-recommends -y \
  python-pip \
  python-setuptools \
  wget

ADD required-packages/trusty-pip.txt .
RUN cat trusty-pip.txt | grep -v "#" | xargs pip install 

ADD required-packages/trusty-dist.txt .
RUN cat trusty-dist.txt | grep -v "#" | xargs apt-get install --no-install-recommends -y

ADD required-packages/trusty-obs.txt .
RUN ./functions.sh add_obs_repo_debian xUbuntu_14.04
RUN cat trusty-obs.txt | grep -v "#" | xargs apt-get install --no-install-recommends -y


# grab gosu for easy step-down from root
ADD helpers/gosu.pubkey /tmp
RUN ./functions.sh step_down_from_root_with_gosu $(dpkg --print-architecture)


# mount points for source code
RUN mkdir /source
VOLUME /source
VOLUME /build


ADD helpers/entrypoint-debian.sh /
ENTRYPOINT ["/entrypoint-debian.sh"]
