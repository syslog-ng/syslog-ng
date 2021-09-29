ARG CONTAINER_REGISTRY
FROM $CONTAINER_REGISTRY/dbld-debian-testing:latest

ARG ARG_IMAGE_PLATFORM
ARG COMMIT
ENV IMAGE_PLATFORM ${ARG_IMAGE_PLATFORM}
LABEL COMMIT=${COMMIT}

RUN /dbld/builddeps install_apt_packages
RUN /dbld/builddeps install_pip_packages
