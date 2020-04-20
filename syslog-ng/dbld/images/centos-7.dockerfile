FROM centos:7
LABEL maintainer="Andras Mitzki <andras.mitzki@balabit.com>, Laszlo Szemere <laszlo.szemere@balabit.com>, Balazs Scheidler <balazs.scheidler@oneidentity.com>"

ARG OS_PLATFORM
ARG COMMIT
ENV OS_PLATFORM ${OS_PLATFORM}
LABEL COMMIT=${COMMIT}

COPY helpers/* /helpers/

RUN /helpers/dependencies.sh add_epel_repo
RUN /helpers/dependencies.sh add_copr_repo
RUN /helpers/dependencies.sh install_yum_packages
RUN /helpers/dependencies.sh install_pip_packages

RUN /helpers/dependencies.sh install_criterion
RUN /helpers/dependencies.sh install_gradle
RUN /helpers/dependencies.sh install_gosu amd64
RUN mv /helpers/fake-sudo.sh /usr/bin/sudo

VOLUME /source
VOLUME /build

ENTRYPOINT ["/helpers/entrypoint.sh"]
