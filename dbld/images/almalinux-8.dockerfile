FROM almalinux:8
LABEL maintainer="Laszlo Varady <laszlo.varady@balabit.com>"
ENV OS_DISTRIBUTION=almalinux
ENV OS_DISTRIBUTION_CODE_NAME=8

ARG ARG_IMAGE_PLATFORM
ARG COMMIT
ENV IMAGE_PLATFORM ${ARG_IMAGE_PLATFORM}
LABEL COMMIT=${COMMIT}

COPY images/entrypoint.sh /
COPY . /dbld/

RUN /dbld/builddeps update_packages
RUN /dbld/builddeps install_dbld_dependencies
RUN /dbld/builddeps add_epel_repo
RUN /dbld/builddeps add_copr_repo
RUN /dbld/builddeps install_yum_packages
RUN /dbld/builddeps install_rpm_build_deps

RUN /dbld/builddeps install_criterion

VOLUME /source
VOLUME /build

ENTRYPOINT ["/entrypoint.sh"]
