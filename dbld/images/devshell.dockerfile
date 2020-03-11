FROM balabit/syslog-ng-ubuntu-bionic:latest
ENV OS_PLATFORM devshell


RUN /helpers/dependencies.sh enable_dbgsyms_on_ubuntu
RUN /helpers/dependencies.sh install_perf

RUN /helpers/dependencies.sh install_apt_packages
