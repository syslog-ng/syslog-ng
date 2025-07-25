FROM ubuntu:jammy
ARG ARG_IMAGE_PLATFORM
ARG COMMIT
ARG JENKINS_URL

LABEL maintainer="kira.syslogng@gmail.com"
LABEL org.opencontainers.image.authors="kira.syslogng@gmail.com"
LABEL COMMIT=${COMMIT}

ENV OS_DISTRIBUTION=ubuntu
ENV OS_DISTRIBUTION_CODE_NAME=jammy
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

VOLUME /source
VOLUME /build

ENTRYPOINT ["/entrypoint.sh"]
