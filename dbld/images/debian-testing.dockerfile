FROM debian:testing
ARG ARG_IMAGE_PLATFORM
ARG COMMIT

LABEL maintainer="kira.syslogng@gmail.com"
LABEL org.opencontainers.image.authors="kira.syslogng@gmail.com"
LABEL COMMIT=${COMMIT}

ENV OS_DISTRIBUTION=debian
ENV OS_DISTRIBUTION_CODE_NAME=testing
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
# debian testing does not have criterion in its repo because of continuous regression issues
# also it has build issues with the latest versions, so we install from binary
RUN /dbld/builddeps install_criterion

VOLUME /source
VOLUME /build

ENTRYPOINT ["/entrypoint.sh"]
