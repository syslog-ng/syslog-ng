FROM balabit/syslog-ng-ubuntu-bionic:latest
ENV OS_PLATFORM devshell


RUN /dbld/builddeps enable_dbgsyms_on_ubuntu
RUN /dbld/builddeps install_perf

RUN /dbld/builddeps install_apt_packages
