FROM balabit/syslog-ng-ubuntu-focal:latest
ENV OS_PLATFORM devshell


RUN /dbld/builddeps enable_dbgsyms_on_ubuntu
RUN /dbld/builddeps install_perf

RUN /dbld/builddeps install_apt_packages
RUN /dbld/builddeps install_pip2
RUN /dbld/builddeps install_pip2_packages
RUN /dbld/builddeps install_pip3_packages
