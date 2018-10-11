FROM centos:7
LABEL maintainer="Andras Mitzki <andras.mitzki@balabit.com>, Laszlo Szemere <laszlo.szemere@balabit.com>, Balazs Scheidler <balazs.scheidler@oneidentity.com>"
ENV OS_PLATFORM centos-7

COPY helpers/* /helpers/

RUN /helpers/dependencies.sh install_yum_packages
RUN /helpers/dependencies.sh install_pip_packages

RUN /helpers/dependencies.sh install_criterion
RUN /helpers/dependencies.sh install_gradle
RUN /helpers/dependencies.sh install_gosu amd64
RUN mv /helpers/fake-sudo.sh /usr/bin/sudo

VOLUME /source
VOLUME /build

ENTRYPOINT ["/helpers/entrypoint.sh"]
