FROM balabit/syslog-ng-tarball:latest
ENV OS_PLATFORM devshell

ARG COMMIT
LABEL COMMIT=${COMMIT}


RUN /dbld/builddeps enable_dbgsyms
RUN /dbld/builddeps install_perf

RUN /dbld/builddeps install_apt_packages
RUN /dbld/builddeps install_pip2
RUN /dbld/builddeps install_pip_packages
