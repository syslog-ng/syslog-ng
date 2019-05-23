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

VERSION=$(get_version)
