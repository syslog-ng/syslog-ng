#! /bin/sh

CORE_MODULES="syslog-ng-mod-sql syslog-ng-mod-mongodb"

# Java packages are never built (see dbld/build.manifest), so exclude
# them from Recommends: too, instead of relying on the package simply
# not existing in the repo.
JAVA_DEPENDENT_MODULES="syslog-ng-mod-java syslog-ng-mod-java-common-lib syslog-ng-mod-hdfs"

ALL_MODULES=$(echo $(grep "^Package: syslog-ng-mod-" debian/control | cut -d: -f 2))
for javamod in ${JAVA_DEPENDENT_MODULES}; do
	ALL_MODULES=$(echo ${ALL_MODULES} | tr ' ' '\n' | grep -v "^${javamod}$" | tr '\n' ' ')
done

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
