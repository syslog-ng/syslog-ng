ARG CONTAINER_REGISTRY
ARG CONTAINER_ARCH=amd64
ARG CONTAINER_BASE_OS=debian
ARG CONTAINER_BASE_OS_CODE=trixie
ARG CONTAINER_BASE=$CONTAINER_BASE_OS-$CONTAINER_BASE_OS_CODE
ARG CONTAINER_NAME_SUFFIX
FROM --platform=linux/$CONTAINER_ARCH $CONTAINER_REGISTRY/dbld-$CONTAINER_BASE$CONTAINER_NAME_SUFFIX:latest
ARG ARG_IMAGE_PLATFORM
ARG COMMIT

LABEL maintainer="kira.syslogng@gmail.com"
LABEL org.opencontainers.image.authors="kira.syslogng@gmail.com"
LABEL COMMIT=${COMMIT}

ENV IMAGE_PLATFORM ${ARG_IMAGE_PLATFORM}

RUN /dbld/builddeps enable_dbgsyms
RUN /dbld/builddeps install_perf

RUN /dbld/builddeps install_apt_packages
