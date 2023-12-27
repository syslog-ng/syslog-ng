FROM ubuntu:mantic
LABEL maintainer="Laszlo Varady <laszlo.varady@balabit.com>, Andras Mitzki <andras.mitzki@balabit.com>, Laszlo Szemere <laszlo.szemere@balabit.com>, Balazs Scheidler <balazs.scheidler@oneidentity.com>"
ENV OS_DISTRIBUTION=ubuntu
ENV OS_DISTRIBUTION_CODE_NAME=mantic

ARG ARG_IMAGE_PLATFORM
ARG COMMIT
ENV IMAGE_PLATFORM ${ARG_IMAGE_PLATFORM}
LABEL COMMIT=${COMMIT}

ENV DEBIAN_FRONTEND=noninteractive
ENV DEBCONF_NONINTERACTIVE_SEEN=true
ENV LANG C.UTF-8

COPY images/entrypoint.sh /
COPY . /dbld/

RUN /dbld/builddeps install_dbld_dependencies
RUN /dbld/builddeps install_apt_packages
RUN /dbld/builddeps install_debian_build_deps

VOLUME /source
VOLUME /build

ENTRYPOINT ["/entrypoint.sh"]
