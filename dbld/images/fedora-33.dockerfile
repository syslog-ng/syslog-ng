FROM fedora:33
LABEL maintainer="Andras Mitzki <andras.mitzki@balabit.com>, Laszlo Szemere <laszlo.szemere@balabit.com>, Balazs Scheidler <bazsi77@gmail.com>"
ENV OS_DISTRIBUTION=fedora
ENV OS_DISTRIBUTION_CODE_NAME=33

ARG ARG_IMAGE_PLATFORM
ARG COMMIT
ENV IMAGE_PLATFORM ${ARG_IMAGE_PLATFORM}
LABEL COMMIT=${COMMIT}

COPY images/fake-sudo.sh /usr/bin/sudo
COPY images/entrypoint.sh /
COPY . /dbld/

RUN /dbld/builddeps install_dbld_dependencies
RUN /dbld/builddeps add_copr_repo
RUN /dbld/builddeps install_yum_packages
RUN /dbld/builddeps install_rpm_build_deps
RUN /dbld/builddeps install_pip_packages

RUN /dbld/builddeps install_criterion
RUN /dbld/builddeps install_gradle
RUN /dbld/builddeps install_gosu amd64

VOLUME /source
VOLUME /build

ENTRYPOINT ["/entrypoint.sh"]
