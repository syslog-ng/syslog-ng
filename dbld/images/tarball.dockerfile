ARG CONTAINER_REGISTRY
FROM $CONTAINER_REGISTRY/dbld-debian-bookworm:latest
ARG ARG_IMAGE_PLATFORM
ARG COMMIT

LABEL maintainer="kira.syslogng@gmail.com"
LABEL org.opencontainers.image.authors="kira.syslogng@gmail.com"
LABEL COMMIT=${COMMIT}

ENV IMAGE_PLATFORM ${ARG_IMAGE_PLATFORM}

RUN /dbld/builddeps install_apt_packages
