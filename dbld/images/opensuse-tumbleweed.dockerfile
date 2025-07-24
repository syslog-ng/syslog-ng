FROM opensuse/tumbleweed:latest
ARG ARG_IMAGE_PLATFORM
ARG COMMIT

LABEL maintainer="kira.syslogng@gmail.com"
LABEL org.opencontainers.image.authors="kira.syslogng@gmail.com"
LABEL COMMIT=${COMMIT}

ENV OS_DISTRIBUTION=opensuse
ENV OS_DISTRIBUTION_CODE_NAME=tumbleweed
ENV IMAGE_PLATFORM ${ARG_IMAGE_PLATFORM}

COPY images/entrypoint.sh /
COPY . /dbld/

RUN /dbld/builddeps update_packages
RUN /dbld/builddeps workaround_rpm_repos
RUN /dbld/builddeps install_dbld_dependencies
RUN /dbld/builddeps add_copr_repo
RUN /dbld/builddeps install_zypper_packages
# TODO: we do not have a binary package for syslog-ng on openSUSE now, fix the install_rpm_build_deps to support if a package is needed in the future
#RUN /dbld/builddeps install_rpm_build_deps

RUN /dbld/builddeps install_gradle
# RUN /dbld/builddeps install_bison_from_source

VOLUME /source
VOLUME /build

ENTRYPOINT ["/entrypoint.sh"]
