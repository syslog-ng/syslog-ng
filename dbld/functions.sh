set -e

function get_version() {
	cd /source
	scripts/version.sh
}

VERSION=$(get_version)
