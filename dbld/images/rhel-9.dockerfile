# FIXME: once we have a proper licence, use the ubi version of the image
#FROM registry.access.redhat.com/ubi9/ubi:latest
FROM almalinux:9
ARG ARG_IMAGE_PLATFORM
ARG COMMIT

LABEL maintainer="kira.syslogng@gmail.com"
LABEL org.opencontainers.image.authors="kira.syslogng@gmail.com"
LABEL COMMIT=${COMMIT}

ENV OS_DISTRIBUTION=rhel
ENV OS_DISTRIBUTION_CODE_NAME=9
ENV IMAGE_PLATFORM ${ARG_IMAGE_PLATFORM}

COPY images/entrypoint.sh /
COPY . /dbld/

RUN /dbld/builddeps update_packages
RUN /dbld/builddeps workaround_rpm_repos
RUN /dbld/builddeps install_dbld_dependencies
RUN /dbld/builddeps add_epel_repo
RUN /dbld/builddeps add_copr_repo
RUN /dbld/builddeps install_dnf_packages
RUN /dbld/builddeps install_rpm_build_deps

RUN /dbld/builddeps install_criterion_from_source
RUN /dbld/builddeps install_gradle
# bison is too old, at least version 3.7.6 is required
RUN /dbld/builddeps install_bison_from_source

VOLUME /source
VOLUME /build

ENTRYPOINT ["/entrypoint.sh"]
