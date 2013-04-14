#! /bin/sh

set -e

if [ "$#" -lt 1 ]; then
	cat <<EOF
Usage: $0 UPSTREAM_VERSION features...
EOF
	exit 1
fi

UPSTREAM_VERSION="$1"
UPSTREAM_VERSION_MAJOR="$(echo "$1" | cut -d "." -f 1,2)"

shift
enabled_fetures=""
# Construct the regexp for enabled features...
for f in $@; do
        enabled_features="$enabled_features -e s#@IF_ENABLED($f):\(.*\)@#\1#"
done

# Update debian/control
sed -e "s,@UPSTREAM_VERSION@,${UPSTREAM_VERSION},g" \
    -e "s,@UPSTREAM_VERSION_MAJOR@,${UPSTREAM_VERSION_MAJOR},g" \
    $enabled_features \
    -e "/@IF_ENABLED/d" \
    < debian/control.d/control.in \
    > debian/control

# Update debian/syslog-ng-core.dirs and .links
for file in syslog-ng-core.dirs syslog-ng-core.links; do
        rm -f debian/${file}
        for feature in default $@; do
                if [ -e debian/control.d/${feature}/${file} ]; then
                        cat debian/control.d/${feature}/${file} >>debian/${file}
                fi
        done
done
