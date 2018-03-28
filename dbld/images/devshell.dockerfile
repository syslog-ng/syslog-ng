FROM balabit/syslog-ng-artful:latest
ARG DISTRO=artful

LABEL maintainer="Andras Mitzki <andras.mitzki@balabit.com>, Laszlo Szemere <laszlo.szemere@balabit.com>"

COPY helpers/* /helpers/

RUN /helpers/functions.sh enable_dbgsyms
RUN /helpers/functions.sh install_perf

COPY devshell-pip/all.txt devshell-pip/${DISTRO}*.txt /devshell-pip/
RUN cat /devshell-pip/* | grep -v '^$\|^#' | xargs pip install

COPY devshell-apt/all.txt devshell-apt/${DISTRO}*.txt /devshell-apt/
RUN cat /devshell-apt/* | grep -v '^$\|^#' | xargs apt-get install --no-install-recommends -y
