FROM balabit/syslog-ng-ubuntu-xenial
LABEL maintainer="Andras Mitzki <andras.mitzki@balabit.com>, Laszlo Szemere <laszlo.szemere@balabit.com>, Balazs Scheidler <balazs.scheidler@oneidentity.com>"

ENV OS_PLATFORM kira

RUN /dbld/builddeps install_apt_packages
RUN /dbld/builddeps install_pip_packages
