FROM debian:testing
LABEL maintainer="Andras Mitzki <andras.mitzki@balabit.com>"


RUN apt-get update -qq && apt-get install -y \
    wget \
    ca-certificates \
    gnupg2

RUN wget -qO - https://ose-repo.syslog-ng.com/apt/syslog-ng-ose-pub.asc | gpg --dearmor > /usr/share/keyrings/ose-repo-archive-keyring.gpg && \
  echo "deb [ signed-by=/usr/share/keyrings/ose-repo-archive-keyring.gpg ] https://ose-repo.syslog-ng.com/apt/ stable debian-testing" | tee --append /etc/apt/sources.list.d/syslog-ng-ose.list

RUN apt-get update -qq && apt-get install -y \
    libdbd-mysql libdbd-pgsql libdbd-sqlite3 syslog-ng

ADD syslog-ng.conf /etc/syslog-ng/syslog-ng.conf

EXPOSE 514/udp
EXPOSE 601/tcp
EXPOSE 6514/tcp

HEALTHCHECK --interval=2m --timeout=3s --start-period=30s CMD /usr/sbin/syslog-ng-ctl stats || exit 1




ENTRYPOINT ["/usr/sbin/syslog-ng", "-F"]
