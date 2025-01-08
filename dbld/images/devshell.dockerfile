ARG CONTAINER_REGISTRY
FROM $CONTAINER_REGISTRY/dbld-debian-bookworm:latest

ARG ARG_IMAGE_PLATFORM
ARG COMMIT
ENV IMAGE_PLATFORM=${ARG_IMAGE_PLATFORM}
LABEL COMMIT=${COMMIT}


RUN /dbld/builddeps enable_dbgsyms
RUN /dbld/builddeps install_perf

RUN /dbld/builddeps install_apt_packages
