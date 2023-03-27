ARG SYSLOGNG_VERSION=latest
FROM ghcr.io/axoflow/syslog-ng:${SYSLOGNG_VERSION}
LABEL maintainer="László Várady <laszlo.varady@axoflow.com>"

ADD splunk-connect-for-syslog/package/etc /etc/syslog-ng
ADD splunk-connect-for-syslog/pyproject.toml splunk-connect-for-syslog/poetry.lock entrypoint.sh /
ADD splunk-connect-for-syslog/package/sbin /sbin

RUN apk add -U --upgrade --no-cache \
      bash \
      build-base \
      curl \
      grep \
      less \
      net-tools \
      netcat-openbsd \
      openssl \
      procps \
      py3-pip \
      python3 \
      python3-dev \
      libffi-dev \
      shadow \
      socat \
      tzdata \
      wget \
    && curl -fsSL https://goss.rocks/install | GOSS_VER=v0.3.21 sh \
    && groupadd --gid 1024 syslog \
    && useradd -M -g 1024 -u 1024 syslog \
    && usermod -L syslog \
    && touch /var/log/syslog-ng.out /var/log/syslog-ng.err \
    && chmod 755 /var/log/syslog-ng.* \
    && pip3 --no-cache-dir install poetry \
    && poetry export --format requirements.txt \
        | /var/lib/syslog-ng/python-venv/bin/pip3 --no-cache-dir install -r /dev/stdin \
    && apk del build-base python3-dev libffi-dev

HEALTHCHECK --interval=10s --retries=6 --timeout=6s CMD /sbin/healthcheck.sh
ENTRYPOINT ["/entrypoint.sh"]
