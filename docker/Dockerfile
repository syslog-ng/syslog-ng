#############################################################################
# Copyright (c) 2015-2023 Balabit
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
# As an additional exemption you are allowed to compile & link against the
# OpenSSL libraries as published by the OpenSSL project. See the file
# COPYING for details.
#
#############################################################################

FROM debian:testing
LABEL maintainer="Andras Mitzki <andras.mitzki@balabit.com>, László Várady <laszlo.varady@balabit.com>"
LABEL org.opencontainers.image.source="https://github.com/syslog-ng/syslog-ng"

ARG PKG_TYPE=stable

RUN apt-get update -qq && apt-get install -y \
    wget \
    ca-certificates \
    gnupg2 \
    && rm -rf /var/lib/apt/lists/*

RUN wget -qO - https://ose-repo.syslog-ng.com/apt/syslog-ng-ose-pub.asc | gpg --dearmor > /usr/share/keyrings/ose-repo-archive-keyring.gpg && \
  echo "deb [signed-by=/usr/share/keyrings/ose-repo-archive-keyring.gpg] https://ose-repo.syslog-ng.com/apt/ ${PKG_TYPE} debian-testing" | tee --append /etc/apt/sources.list.d/syslog-ng-ose.list

RUN apt-get update -qq && apt-get install -y \
    libdbd-mysql libdbd-pgsql libdbd-sqlite3 syslog-ng libjemalloc2 \
    && rm -rf /var/lib/apt/lists/*

ADD syslog-ng.conf /etc/syslog-ng/syslog-ng.conf

EXPOSE 514/udp
EXPOSE 601/tcp
EXPOSE 6514/tcp

HEALTHCHECK --interval=2m --timeout=5s --start-period=30s CMD /usr/sbin/syslog-ng-ctl healthcheck --timeout 5
ENV LD_PRELOAD /usr/lib/x86_64-linux-gnu/libjemalloc.so.2
ENTRYPOINT ["/usr/sbin/syslog-ng", "-F"]
