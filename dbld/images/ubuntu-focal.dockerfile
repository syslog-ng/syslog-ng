FROM ubuntu:focal
ARG ARG_IMAGE_PLATFORM
ARG COMMIT
ARG JENKINS_URL

LABEL maintainer="kira.syslogng@gmail.com"
LABEL org.opencontainers.image.authors="kira.syslogng@gmail.com"
LABEL COMMIT=${COMMIT}

ENV OS_DISTRIBUTION=ubuntu
ENV OS_DISTRIBUTION_CODE_NAME=focal
ENV IMAGE_PLATFORM ${ARG_IMAGE_PLATFORM}
ENV DEBIAN_FRONTEND=noninteractive
ENV DEBCONF_NONINTERACTIVE_SEEN=true
ENV LANG C.UTF-8

COPY images/entrypoint.sh /
COPY . /dbld/

RUN /dbld/builddeps update_packages
RUN /dbld/builddeps install_dbld_dependencies
RUN /dbld/builddeps install_apt_packages
RUN /dbld/builddeps install_debian_build_deps

RUN /dbld/builddeps install_criterion
# bison is too old, at least version 3.7.6 is required
RUN /dbld/builddeps install_bison_from_source

VOLUME /source
VOLUME /build

ENTRYPOINT ["/entrypoint.sh"]
