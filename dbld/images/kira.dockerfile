ARG CONTAINER_REGISTRY
FROM $CONTAINER_REGISTRY/dbld-ubuntu-focal
LABEL maintainer="Andras Mitzki <andras.mitzki@balabit.com>, Laszlo Szemere <laszlo.szemere@balabit.com>, Balazs Scheidler <balazs.scheidler@oneidentity.com>"

ARG ARG_IMAGE_PLATFORM
ARG COMMIT
ENV IMAGE_PLATFORM ${ARG_IMAGE_PLATFORM}
LABEL COMMIT=${COMMIT}

RUN /dbld/builddeps install_apt_packages
RUN /dbld/builddeps install_bison_from_source
RUN /dbld/builddeps install_pip2
RUN /dbld/builddeps install_pip_packages
