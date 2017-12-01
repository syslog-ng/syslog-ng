FROM ubuntu:17.04
MAINTAINER Andras Mitzki <andras.mitzki@balabit.com>

RUN apt-get update -qq && apt-get install -y \
    wget

RUN wget -qO - http://download.opensuse.org/repositories/home:/laszlo_budai:/syslog-ng/xUbuntu_17.04/Release.key | apt-key add -
RUN echo 'deb http://download.opensuse.org/repositories/home:/laszlo_budai:/syslog-ng/xUbuntu_17.04 ./' | tee --append /etc/apt/sources.list.d/syslog-ng-obs.list

ADD dev-dependencies.txt .
RUN apt-get update -qq && cat dev-dependencies.txt | grep -v "#" | xargs apt-get install -y --allow-unauthenticated

#
# if you want to add further packages, add them to addons.txt to make image
# creation faster.  If you are done, move them to dev-dependencies and leave
# addons.txt empty.  The image creation will be faster, as the results of
# the original dev-dependencies will be reused by docker.
#
ADD addons.txt .
RUN apt-get update -qq && cat addons.txt | grep -v "#" | xargs apt-get install -y

RUN cd /tmp && wget http://ftp.de.debian.org/debian/pool/main/libn/libnative-platform-java/libnative-platform-jni_0.11-5_$(dpkg --print-architecture).deb
RUN cd /tmp && dpkg -i libnative-platform-jni*.deb

ADD gosu.pubkey /tmp
# grab gosu for easy step-down from root
RUN (cat /tmp/gosu.pubkey | gpg --import) \
    && wget -O /usr/local/bin/gosu "https://github.com/tianon/gosu/releases/download/1.7/gosu-$(dpkg --print-architecture)" \
    && wget -O /usr/local/bin/gosu.asc "https://github.com/tianon/gosu/releases/download/1.7/gosu-$(dpkg --print-architecture).asc" \
    && gpg --verify /usr/local/bin/gosu.asc \
    && rm /usr/local/bin/gosu.asc \
    && chmod +x /usr/local/bin/gosu


ADD entrypoint.sh /
ENTRYPOINT ["/entrypoint.sh"]
RUN mkdir /source
VOLUME /source
VOLUME /build
