set -e

function get_version() {
	cd /source
	scripts/version.sh
}

function run_build_command() {
	OS=$(echo $OS_PLATFORM |  cut -d '-' -f 1)

	# we sort longer strings in front, to make more specific items
	# first.

	TMPDIR=$(mktemp -d)

	IFS=$'\t'
	egrep -e "^${OS}([^-]|$)" -e "^${OS_PLATFORM}" /source/dbld/build.manifest | sort -r | head -1 | while read os env cmdline; do
		unset IFS
		declare -a env_values
		if [ "${env}" != "-" -a "${env}" != "" ]; then
			echo "${env}" | tr ',' '\n' | xargs -n1 echo > $TMPDIR/env.list
		        readarray -t env_values < $TMPDIR/env.list
		fi
		echo "Running build as: " env "${env_values[@]}" "$@" $cmdline
		env "${env_values[@]}" "$@" $cmdline
	done
}

function validate_man_binary() {
	MAN=`which man`
	if ! [ -x ${MAN} ]; then
		return 0
	fi

	set +e
	man_help=$(${MAN} --help 2>&1)
	rc=$?
	set -e
	if [ "$rc" -ne 0 ]; then
		cat <<EOF
${man_help}

Your /usr/bin/man binary seems disfunctional within the dbld container.
This is a dependency of debhelper, which is required to generate syslog-ng
Debian packages.

This may happen because of an AppArmor bug that you can work around by
removing the apparmor policy for man on the HOST computer, invoking this
command as root:

bash# apparmor_parser -R /etc/apparmor.d/usr.bin.man

The error that happens is that man is complaining about a missing
libmandb.so, which is there, but AppArmor prevents access.

You can validate whether this was successful by querying the running
apparmor policy:

bash# apparmor_status
EOF
		return 1
	fi
}

function validate_container() {
	validate_man_binary
}

VERSION=$(get_version)
