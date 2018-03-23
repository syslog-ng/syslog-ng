FROM centos:7
ARG DISTRO=centos7

LABEL maintainer="Andras Mitzki <andras.mitzki@balabit.com>, Laszlo Szemere <laszlo.szemere@balabit.com>"

COPY helpers/* /helpers/

#Install packages
RUN yum install -y \
  wget \
  epel-release

RUN yum install -y \
  python-pip \
  python-setuptools

COPY required-pip/all.txt required-pip/${DISTRO}*.txt /required-pip/
RUN cat /required-pip/* | grep -v '^$\|^#' | xargs pip install


COPY required-yum/all.txt required-yum/${DISTRO}*.txt /required-yum/
RUN cat /required-yum/* | grep -v '^$\|^#' | xargs yum install -y

RUN /helpers/functions.sh add_epel_repo ${DISTRO}
COPY required-epel/all.txt required-epel/${DISTRO}*.txt /required-epel/
RUN cat /required-epel/* | grep -v '^$\|^#' | xargs yum install -y


RUN /helpers/functions.sh gradle_installer
RUN echo "/usr/lib/jvm/jre/lib/amd64/server" | tee --append /etc/ld.so.conf.d/openjdk-libjvm.conf && ldconfig


# grab gosu for easy step-down from root
RUN /helpers/functions.sh step_down_from_root_with_gosu amd64


# add a fake sudo command
RUN mv /helpers/fake-sudo.sh /usr/bin/sudo


# mount points for source code
RUN mkdir /source
VOLUME /source
VOLUME /build


ENTRYPOINT ["/helpers/entrypoint.sh"]
