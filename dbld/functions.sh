set -e

function get_version() {
	[ -n "$VERSION" ] && echo $VERSION && return
	[ -d /source/scripts ] && cd /source && scripts/version.sh || echo "unknown-version"
}

function _map_cmdline_null()
{
	echo -n ""
}

function _map_env_null()
{
	echo "_=_"
}

function run_build_command_with_build_manifest_parameters() {
	map_environment=$1
	map_cmdline=$2
	shift 2

	# we sort longer strings in front, to make more specific items
	# first.

	TMPDIR=$(mktemp -d)

	IFS=$'\t'
	egrep -e "^${OS_DISTRIBUTION}([^-]|$)" -e "^${IMAGE_PLATFORM}" /dbld/build.manifest | sort -r | head -1 | while read os featureflags env cmdline; do
		unset IFS
		echo $os $featureflags $env $cmdline
		declare -a env_values
		if [ "${env}" != "-" -a "${env}" != "" ]; then
			# xargs processes removes quote characters just like the shell
			echo "${env}" | tr ',' '\n' | xargs -n1 echo > $TMPDIR/env.list
		        readarray -t env_values < $TMPDIR/env.list
		fi

		if [ "`${map_cmdline} ${featureflags}`" = "" ]; then

			# no extra command line values (e.g.  deb), execute
			# it without them on the env command line.  In this
			# case we supplied an extra, zero-length argument to
			# dpkg-buildpackage that it errored out on (understandably).
			#
			# I couldn't find a way to expand a variable in shell that
			#   1) was able to handle spaces in arguments
			#   2) could also represent no extra arguments (not even an empty one)
			#
			# that's why we are using this ugly if statement above and the
			# double evaluation of ${map_cmdline}.
			#

			echo "Runnign build as: " env "${env_values[@]}" "`${map_environment} ${featureflags}`" "$@" $cmdline
			env "${env_values[@]}" "`${map_environment} ${featureflags}`" "$@" $cmdline
		else
			${map_cmdline} ${featureflags} | xargs -n1 echo > $TMPDIR/cmdline.list
			readarray -t cmdline_values < $TMPDIR/cmdline.list
			echo "Running build as: " env "${env_values[@]}" "`${map_environment} ${featureflags}`" "$@" $cmdline "${cmdline_values[@]}"
			env "${env_values[@]}" "`${map_environment} ${featureflags}`" "$@" $cmdline "${cmdline_values[@]}"
		fi
	done
}

function _map_feature_flags_to_deb_build_profiles()
{
    echo -n DEB_BUILD_PROFILES=
    IFS=,
    for feature in $1; do
        case "$feature" in
          nojava|nopython)
            # these are standard Debian build profiles, keep them intact
            echo -n "$feature "
            ;;
          *)
            # everything else is prefixed with "sng-"
            echo -n "sng-$feature "
            ;;
        esac
    done
    echo
}

function deb_run_build_command()
{
    run_build_command_with_build_manifest_parameters _map_feature_flags_to_deb_build_profiles _map_cmdline_null "$@"
}

function _map_feature_flags_to_rpmbuild_with_and_without_options()
{
    IFS=,
    echo -n "--define='_dbld 1' "
    for feature in $1; do
        case "$feature" in
            no*)
                feature_without_no=`echo $feature | sed -e 's/^no//'`
                echo -n "--define='_without_${feature_without_no} --without-${feature_without_no}' "
                ;;
            *)
                echo -n "--define='_with_${feature} --with-${feature}' "
                ;;
        esac
    done
}


function rpm_run_build_command()
{
    run_build_command_with_build_manifest_parameters _map_env_null _map_feature_flags_to_rpmbuild_with_and_without_options "$@"
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

function capture_artifacts()
{
	if [ "$MODE" = "release" ]; then
		ARTIFACT_DIR=${ARTIFACT_DIR:-/dbld/release/$VERSION}
		echo "Capturing artifacts:" "$*" "into" "${ARTIFACT_DIR}"
		cp -R -v "$*" "${ARTIFACT_DIR}"
		echo "The current list of artifacts:"
		ls -l "${ARTIFACT_DIR}"
	fi
}

VERSION=$(get_version)
