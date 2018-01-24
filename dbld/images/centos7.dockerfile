FROM centos:7
MAINTAINER Andras Mitzki <andras.mitzki@balabit.com>

RUN yum install -y wget epel-release

RUN cd /etc/yum.repos.d && wget https://copr.fedorainfracloud.org/coprs/czanik/syslog-ng311/repo/epel-7/czanik-syslog-ng311-epel-7.repo

ADD dev-dependencies.txt .
RUN cat dev-dependencies.txt | grep -v "#" | xargs yum -y  install
ADD gradle_installer.sh .
RUN ./gradle_installer.sh
RUN echo "/usr/lib/jvm/jre/lib/amd64/server" | tee --append /etc/ld.so.conf.d/openjdk-libjvm.conf && ldconfig

#
# if you want to add further packages, add them to addons.txt to make image
# creation faster.  If you are done, move them to dev-dependencies and leave
# addons.txt empty.  The image creation will be faster, as the results of
# the original dev-dependencies will be reused by docker.
#
ADD addons.txt .
RUN cat addons.txt | grep -v "#" | xargs -r yum -y install

ADD gosu.pubkey /tmp
# grab gosu for easy step-down from root
RUN (cat /tmp/gosu.pubkey | gpg --import) \
    && wget -O /usr/local/bin/gosu "https://github.com/tianon/gosu/releases/download/1.7/gosu-amd64" \
    && wget -O /usr/local/bin/gosu.asc "https://github.com/tianon/gosu/releases/download/1.7/gosu-amd64.asc" \
    && gpg --verify /usr/local/bin/gosu.asc \
    && rm /usr/local/bin/gosu.asc \
    && chmod +x /usr/local/bin/gosu


ADD entrypoint.sh /
ENTRYPOINT ["/entrypoint.sh"]
RUN mkdir /source
VOLUME /source
VOLUME /build
