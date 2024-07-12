FROM quay.io/centos/centos:stream9
LABEL org.opencontainers.image.authors="kira.syslogng@gmail.com"
ENV OS_DISTRIBUTION=centos
ENV OS_DISTRIBUTION_CODE_NAME=stream9

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

RUN dnf upgrade -y

VOLUME /source
VOLUME /build

ENTRYPOINT ["/entrypoint.sh"]
