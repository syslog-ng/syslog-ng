FROM balabit/syslog-ng-artful:latest
ARG DISTRO=artful

LABEL maintainer="Andras Mitzki <andras.mitzki@balabit.com>, Laszlo Szemere <laszlo.szemere@balabit.com>"

RUN /helpers/functions.sh enable_dbgsyms
RUN /helpers/functions.sh install_perf

COPY required-pip/devshell.txt /devshell-pip/
RUN cat /devshell-pip/* | grep -v '^$\|^#' | xargs pip install

RUN apt-get install --no-install-recommends -y \
  gdb \
  lsof \
  strace \
  vim \
  joe \
  netcat \
  virtualenv \
  valgrind \
  linux-tools-generic \
  libjemalloc-dev \
  locales \
  lcov \
  gdbserver \
  libriemann-client-dev \
  libdbi-dev \
  libdbd-sqlite3 \
  clang \
  \
  libc6-dbg \
  libpcre2-dbg \
  libglib2.0-0-dbgsym \
  python-dbg \
  python3-dbg \
  libssl1.0.0-dbg \
  libjemalloc1-dbgsym

RUN echo en_US.UTF-8 UTF-8 > /etc/locale.gen && locale-gen
