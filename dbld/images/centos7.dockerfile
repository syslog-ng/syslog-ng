FROM centos:7
MAINTAINER Andras Mitzki <andras.mitzki@balabit.com>, Laszlo Szemere <laszlo.szemere@balabit.com>

ADD helpers/functions.sh .

RUN yum install -y \
#  python-pip \
#  python-setuptools \
  wget \
  epel-release

#ADD required-packages/centos7-pip.txt .
#RUN cat centos7-pip.txt | grep -v "#" | xargs pip install 

ADD required-packages/centos7-dist.txt .
RUN cat centos7-dist.txt | grep -v "#" | xargs yum install -y

ADD required-packages/centos7-czanik.txt .
RUN ./functions.sh add_czanik_repo_centos7
RUN cat centos7-czanik.txt | grep -v "#" | xargs yum install -y

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
