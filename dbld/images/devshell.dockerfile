ARG CONTAINER_REGISTRY
FROM $CONTAINER_REGISTRY/dbld-tarball:latest

ARG ARG_IMAGE_PLATFORM
ARG COMMIT
ENV IMAGE_PLATFORM ${ARG_IMAGE_PLATFORM}
LABEL COMMIT=${COMMIT}


RUN /dbld/builddeps enable_dbgsyms
RUN /dbld/builddeps install_perf

RUN /dbld/builddeps install_apt_packages
RUN /dbld/builddeps install_pip2
RUN /dbld/builddeps install_pip_packages
