#############################################################################
# Copyright (c) 2015-2023 Balabit
# Copyright (c) 2024 One Identity LLC.
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

FROM debian:trixie
LABEL org.opencontainers.image.authors="kira.syslogng@gmail.com"
LABEL org.opencontainers.image.source="https://github.com/syslog-ng/syslog-ng"

ARG PKG_TYPE=stable
ARG CONTAINER_ARCH=amd64
ARG CONTAINER_NAME_SUFFIX

RUN apt-get update -qq && apt-get install -y \
    wget \
    ca-certificates \
    gnupg2 \
    && rm -rf /var/lib/apt/lists/*

# See .github/workflows/publish-docker-image.yml for why cannot use the ARG versions of these here currenty.
RUN ARCH=$(arch) && \
  if [ "$ARCH" = "aarch64" ]; then \
    CONTAINER_ARCH="arm64"; CONTAINER_NAME_SUFFIX="-arm64"; \
  else \
    CONTAINER_ARCH="amd64"; CONTAINER_NAME_SUFFIX=""; \
  fi && \
  wget -qO - https://ose-repo.syslog-ng.com/apt/syslog-ng-ose-pub.asc | gpg --dearmor > /usr/share/keyrings/ose-repo-archive-keyring.gpg && \
  echo "deb [signed-by=/usr/share/keyrings/ose-repo-archive-keyring.gpg arch=$CONTAINER_ARCH] https://ose-repo.syslog-ng.com/apt/ ${PKG_TYPE} debian-bookworm$CONTAINER_NAME_SUFFIX" | tee --append /etc/apt/sources.list.d/syslog-ng-ose.list

RUN apt-get update -qq && apt-get install -y \
    libdbd-mysql libdbd-pgsql libdbd-sqlite3 syslog-ng libjemalloc2 \
    && rm -rf /var/lib/apt/lists/*

ENV DEBIAN_FRONTEND noninteractive
RUN apt-get upgrade -y

ADD syslog-ng.conf /etc/syslog-ng/syslog-ng.conf

EXPOSE 514/udp
EXPOSE 601/tcp
EXPOSE 6514/tcp

HEALTHCHECK --interval=2m --timeout=5s --start-period=30s CMD /usr/sbin/syslog-ng-ctl healthcheck --timeout 5

ENV LD_PRELOAD=""

COPY entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
