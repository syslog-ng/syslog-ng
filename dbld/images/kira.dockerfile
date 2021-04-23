FROM balabit/syslog-ng-ubuntu-focal
LABEL maintainer="Andras Mitzki <andras.mitzki@balabit.com>, Laszlo Szemere <laszlo.szemere@balabit.com>, Balazs Scheidler <balazs.scheidler@oneidentity.com>"

ENV OS_PLATFORM kira

RUN /dbld/builddeps install_apt_packages
RUN /dbld/builddeps install_bison_from_source
RUN /dbld/builddeps install_pip2
RUN /dbld/builddeps install_pip_packages
