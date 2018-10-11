FROM debian:stretch
ARG DISTRO=stretch
ARG OBS_REPO=Debian_9.0

LABEL maintainer="Andras Mitzki <andras.mitzki@balabit.com>, Laszlo Szemere <laszlo.szemere@balabit.com>"

COPY helpers/* /helpers/

# Install packages
RUN apt-get update -qq && apt-get install --no-install-recommends -y \
  python-pip \
  python-setuptools \
  wget

COPY required-pip/all.txt required-pip/${DISTRO}*.txt /required-pip/
RUN cat /required-pip/* | grep -v '^$\|^#' | xargs pip install

COPY required-apt/all.txt required-apt/${DISTRO}*.txt /required-apt/
RUN cat /required-apt/* | grep -v '^$\|^#' | xargs apt-get install --no-install-recommends -y

RUN /helpers/functions.sh add_obs_repo ${OBS_REPO}
COPY required-obs/all.txt required-obs/${DISTRO}*.txt /required-obs/
RUN cat /required-obs/* | grep -v '^$\|^#' | xargs apt-get install --no-install-recommends -y

RUN /helpers/dependencies.sh install_gosu amd64

# add a fake sudo command
RUN mv /helpers/fake-sudo.sh /usr/bin/sudo


# mount points for source code
RUN mkdir /source
VOLUME /source
VOLUME /build


ENTRYPOINT ["/helpers/entrypoint.sh"]
