FROM balabit/syslog-ng-debian-testing:latest

ARG OS_PLATFORM
ARG COMMIT
ENV OS_PLATFORM tarball
LABEL COMMIT=${COMMIT}

RUN /dbld/builddeps install_apt_packages
RUN /dbld/builddeps install_pip_packages
