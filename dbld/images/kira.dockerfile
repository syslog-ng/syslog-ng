ARG CONTAINER_REGISTRY
FROM $CONTAINER_REGISTRY/dbld-ubuntu-focal
ARG ARG_IMAGE_PLATFORM
ARG COMMIT

LABEL maintainer="kira.syslogng@gmail.com"
LABEL org.opencontainers.image.authors="kira.syslogng@gmail.com"
LABEL COMMIT=${COMMIT}

ENV IMAGE_PLATFORM ${ARG_IMAGE_PLATFORM}

RUN /dbld/builddeps install_apt_packages
RUN /dbld/builddeps install_bison_from_source
RUN /dbld/builddeps install_pip2
RUN /dbld/builddeps install_pip_packages
RUN /dbld/builddeps set_jvm_paths
