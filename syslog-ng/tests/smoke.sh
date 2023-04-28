#!/usr/bin/env bash
set -e

DOCKER="${DOCKER:-docker}"
SYSLOG_NG_IMAGE="${SYSLOG_NG_IMAGE:-ghcr.io/axoflow/axosyslog:latest}"
SYSLOG_NG_VERSION="${SYSLOG_NG_VERSION:-}"


echo "=> Smoke testing image ${SYSLOG_NG_IMAGE}..."

echo "--> Check --syntax-only"
$DOCKER run --rm "${SYSLOG_NG_IMAGE}" --syntax-only

echo "--> Check --module-registry"
$DOCKER run --rm "${SYSLOG_NG_IMAGE}" --module-registry

if [[ -z "$SYSLOG_NG_VERSION" ]]; then
    echo "--> Skipping syslog-ng version validation"
else
    echo "--> Validate syslog-ng version"
    $DOCKER run --rm "${SYSLOG_NG_IMAGE}" -V | grep "Installer-Version: .*${SYSLOG_NG_VERSION}"
fi

echo "--> I/O test"
conf_version_line="$($DOCKER run --rm "${SYSLOG_NG_IMAGE}" -V | grep 'Config version:')"
config_version="$(echo $conf_version_line | sed -E 's|Config version: ([0-9]\.[0-9])|\1|')"
printf "@version: ${config_version}\n log { source { stdin(); }; destination { file("/dev/stdout"); }; };" > test.conf
echo "test msg" \
    | $DOCKER run -i --rm -v $(realpath test.conf):/etc/syslog-ng/syslog-ng.conf "${SYSLOG_NG_IMAGE}" -F \
    | grep "test msg"

echo "=> Smoke test (${SYSLOG_NG_IMAGE}) passed"
