FROM debian:testing
LABEL maintainer="Andras Mitzki <andras.mitzki@balabit.com>, László Várady <laszlo.varady@balabit.com>"
LABEL org.opencontainers.image.source="https://github.com/syslog-ng/syslog-ng"

ARG PKG_TYPE=stable

RUN apt update -qq && apt install -y wget ca-certificates gnupg2 libdbd-mysql libdbd-pgsql libdbd-sqlite3 syslog-ng libjemalloc2 && rm -rf /var/lib/apt/lists/*&& rm -rf /var/lib/apt/lists/* && \
wget -qO - https://ose-repo.syslog-ng.com/apt/syslog-ng-ose-pub.asc | gpg --dearmor > /usr/share/keyrings/ose-repo-archive-keyring.gpg && \
echo "deb [signed-by=/usr/share/keyrings/ose-repo-archive-keyring.gpg] https://ose-repo.syslog-ng.com/apt/ ${PKG_TYPE} debian-testing" | tee --append /etc/apt/sources.list.d/syslog-ng-ose.list

ADD syslog-ng.conf /etc/syslog-ng/syslog-ng.conf

EXPOSE 514/udp 601/tcp 6514/tcp

HEALTHCHECK --interval=2m --timeout=5s --start-period=30s CMD /usr/sbin/syslog-ng-ctl healthcheck --timeout 5
ENV LD_PRELOAD /usr/lib/x86_64-linux-gnu/libjemalloc.so.2
ENTRYPOINT ["/usr/sbin/syslog-ng", "-F"]
