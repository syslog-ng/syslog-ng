#! /bin/sh

CORE_MODULES="syslog-ng-mod-sql syslog-ng-mod-mongodb"

ALL_MODULES=$(echo $(grep "^Package: syslog-ng-mod-" debian/control | cut -d: -f 2))

# Mirrors the nojava DH_OPTIONS -N... filtering in debian/rules.
if echo " ${DEB_BUILD_PROFILES} " | grep -q ' nojava '; then
	JAVA_DEPENDENT_MODULES="syslog-ng-mod-java syslog-ng-mod-java-common-lib syslog-ng-mod-hdfs"
	for javamod in ${JAVA_DEPENDENT_MODULES}; do
		ALL_MODULES=$(echo ${ALL_MODULES} | tr ' ' '\n' | grep -v "^${javamod}$" | tr '\n' ' ')
	done
fi

case "$1" in
        "core")
                echo ${CORE_MODULES} | tr ' ' ','
                ;;
        "all")
                echo ${ALL_MODULES}  | tr ' ' ','
                ;;
        "optional"|"")
                OPTIONAL_MODULES=""
                for mod in ${ALL_MODULES}; do
                        if ! (echo "${CORE_MODULES}" | grep -q ${mod}); then
                                OPTIONAL_MODULES="${OPTIONAL_MODULES}${mod} "
                        fi
                done
                OPTIONAL_MODULES=$(echo ${OPTIONAL_MODULES})
                echo ${OPTIONAL_MODULES} | tr ' ' ','
                ;;
        *)
                exit 1
                ;;
esac
