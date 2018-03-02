FROM centos:7
MAINTAINER Andras Mitzki <andras.mitzki@balabit.com>, Laszlo Szemere <laszlo.szemere@balabit.com>

ADD helpers/functions.sh .

RUN yum install -y \
#  python-pip \
#  python-setuptools \
  wget \
  epel-release

#ADD required-pip/all.txt pip-all.txt
#RUN cat pip-*.txt | grep -v '^$\|^#' | xargs pip install 

ADD required-yum/all.txt yum-all.txt
RUN cat yum-*.txt | grep -v '^$\|^#' | xargs yum install -y

RUN ./functions.sh add_epel_repo centos7
ADD required-epel/all.txt epel-all.txt
RUN cat epel-*.txt | grep -v '^$\|^#' | xargs yum install -y


RUN ./functions.sh gradle_installer
RUN echo "/usr/lib/jvm/jre/lib/amd64/server" | tee --append /etc/ld.so.conf.d/openjdk-libjvm.conf && ldconfig


# grab gosu for easy step-down from root
ADD helpers/gosu.pubkey /tmp
RUN ./functions.sh step_down_from_root_with_gosu amd64


# add a fake sudo command
ADD helpers/fake-sudo.sh /usr/bin/sudo


# mount points for source code
RUN mkdir /source
VOLUME /source
VOLUME /build


ADD helpers/entrypoint-centos.sh /
ENTRYPOINT ["/entrypoint-centos.sh"]
