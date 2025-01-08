FROM fedora:latest
LABEL org.opencontainers.image.authors="kira.syslogng@gmail.com"
ENV OS_DISTRIBUTION=fedora
ENV OS_DISTRIBUTION_CODE_NAME=latest

ARG ARG_IMAGE_PLATFORM
ARG COMMIT
ENV IMAGE_PLATFORM=${ARG_IMAGE_PLATFORM}
LABEL COMMIT=${COMMIT}

COPY images/entrypoint.sh /
COPY . /dbld/

RUN /dbld/builddeps update_packages
RUN /dbld/builddeps workaround_rpm_repos
RUN /dbld/builddeps install_dbld_dependencies
RUN /dbld/builddeps add_copr_repo
RUN /dbld/builddeps install_yum_packages
RUN /dbld/builddeps install_rpm_build_deps

RUN /dbld/builddeps install_criterion
RUN /dbld/builddeps install_gradle

VOLUME /source
VOLUME /build

ENTRYPOINT ["/entrypoint.sh"]
