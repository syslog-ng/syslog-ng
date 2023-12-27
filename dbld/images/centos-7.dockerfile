FROM centos:7
LABEL maintainer="Andras Mitzki <andras.mitzki@balabit.com>, Laszlo Szemere <laszlo.szemere@balabit.com>, Balazs Scheidler <balazs.scheidler@oneidentity.com>"
ENV OS_DISTRIBUTION=centos
ENV OS_DISTRIBUTION_CODE_NAME=7

ARG ARG_IMAGE_PLATFORM
ARG COMMIT
ENV IMAGE_PLATFORM ${ARG_IMAGE_PLATFORM}
LABEL COMMIT=${COMMIT}

COPY images/entrypoint.sh /
COPY . /dbld/

RUN /dbld/builddeps install_dbld_dependencies
RUN /dbld/builddeps add_epel_repo
RUN /dbld/builddeps add_copr_repo
RUN /dbld/builddeps install_yum_packages
RUN /dbld/builddeps install_rpm_build_deps

RUN /dbld/builddeps install_criterion
RUN /dbld/builddeps install_gradle

VOLUME /source
VOLUME /build

ENTRYPOINT ["/entrypoint.sh"]
