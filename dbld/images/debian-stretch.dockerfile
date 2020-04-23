FROM debian:9
LABEL maintainer="Andras Mitzki <andras.mitzki@balabit.com>, Laszlo Szemere <laszlo.szemere@balabit.com>, Balazs Scheidler <balazs.scheidler@oneidentity.com>"

ARG OS_PLATFORM
ARG COMMIT
ENV OS_PLATFORM ${OS_PLATFORM}
LABEL COMMIT=${COMMIT}

ENV DEBIAN_FRONTEND=noninteractive
ENV DEBCONF_NONINTERACTIVE_SEEN=true
ENV LANG C.UTF-8

COPY images/fake-sudo.sh /usr/bin/sudo
COPY images/entrypoint.sh /
COPY . /dbld/

RUN /dbld/builddeps install_dbld_dependencies
RUN /dbld/builddeps install_apt_packages
RUN /dbld/builddeps install_debian_build_deps
RUN /dbld/builddeps install_pip_packages

RUN /dbld/builddeps install_criterion
RUN /dbld/builddeps install_gradle
RUN /dbld/builddeps install_gosu amd64

VOLUME /source
VOLUME /build

ENTRYPOINT ["/entrypoint.sh"]
