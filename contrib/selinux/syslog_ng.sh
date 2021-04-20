#!/bin/bash

### Exit error codes
#
# 0: everything went fine
# 255: invalid command line-option
# 254: no root privileges
# 253: file or directory does not exist
# 252: policy build failed
# 251: policy install failed
# 250: unsupported os/distribution/version
# 249: conflicting parameters
# 248: internal error at task selection
# 246: syslog-ng not installed
# 244: tty not available

EL_FC=
EL_TE=
OS_VERSION=
INSTALL_PATH="/opt/syslog-ng"
# RHEL8 note: ports 10514/tcp, 10514/udp, 20514/tcp and 20514/udp have been
#  allowed by default
#
# Post-RHEL6.5 note: ports 514/udp, 6514/udp and 6514/tcp are allowed by default
#  if you wish to add further ports, just add them to the end of the list
SYSLOG_NG_TCP_PORTS="601"
SYSLOG_NG_UDP_PORTS="601"
TASK_SELECTED="install_default"
INPUT=

get_console_tty() {
	if is_available tty; then
		CONSOLE_TTY=$( tty )
	else
		echo "The 'tty' binary is not available!" >&2
		exit 244
	fi
}


query_install_path() {
	echo -n "Please enter your installation path for Syslog-ng PE: [${INSTALL_PATH}] "
	read INPUT <"${CONSOLE_TTY}"
}


check_dir() {
	if [ -d "${1}" ]; then
		return 0
	else
		echo "The directory you specified does not exist!" >&2
		return 1
	fi
}


verify_input() {
	INPUT="${INPUT:-${INSTALL_PATH}}"
	echo -n "You have entered '${INPUT}'. Is this correct? [y/N] "
	read ACCEPT <"${CONSOLE_TTY}"
	if [ "x${ACCEPT}x" != "xyx" ]; then return 0; fi
	check_dir "${INPUT}" && return 1 || return 0
}


is_available () {
	which "$1" >/dev/null 2>&1;
}


syslog_ng_is_not_installed() {
	if is_available syslog-ng; then
		return 1
	elif [ -x "${INSTALL_PATH}/sbin/syslog-ng" ]; then
		return 1
	else
		return 0
	fi
}


install_precheck() {
	if syslog_ng_is_not_installed; then
		echo "Syslog-ng does not seem to be installed!" >&2
		exit 246
	fi
}


extract_version_string() {
	sed -n 's:^[a-zA-Z ]\+\([0-9]\+\.[0-9]\+\)\(.[0-9]\+\)\?[a-zA-Z ()]\+$:\1:p'
}


detect_os_version() {
	echo "Detecting RHEL/CentOS/Oracle Linux version..."
	if [ -x "/usr/bin/lsb_release" ]; then
		if lsb_release -d | grep -qE "Description:[[:space:]]+(CentOS|CentOS Linux|Red Hat Enterprise Linux Server|Oracle Linux Server|Enterprise Linux Enterprise Linux Server) release"; then
			OS_VERSION=$( lsb_release -r | cut -f 2 )
		else
			echo "You don't seem to be running a supported Linux distribution!" >&2
			exit 250
		fi
	else
		# The package redhat-lsb-core is most likely not installed...
		if [ -f "/etc/redhat-release" ]; then
			OS_VERSION=$( extract_version_string < "/etc/redhat-release" )
		else
			echo "You don't seem to be running a supported OS!" >&2
			exit 250
		fi
	fi
}


omit_allowed_tcp_ports() {
	sed -e 's:^601::g'
}


omit_allowed_udp_ports() {
	sed -e 's:^601::g'
}


omit_allowed_ports() {
	SYSLOG_NG_TCP_PORTS=$( omit_allowed_tcp_ports <<<"${SYSLOG_NG_TCP_PORTS}" )
	SYSLOG_NG_UDP_PORTS=$( omit_allowed_udp_ports <<<"${SYSLOG_NG_UDP_PORTS}" )
}


setup_vars() {
	echo "Detected RHEL/CentOS/Oracle Linux ${OS_VERSION}."
	case "${OS_VERSION}" in
		5.*)
			
			EL_FC="syslog_ng.el5.fc.in"
			EL_TE="syslog_ng.el5.te.in"
			;;
		6.*)
			EL_FC="syslog_ng.el6.fc.in"
			
			local MINORVER 
			MINORVER=$( cut -d. -f 2 <<<"${OS_VERSION}" )
			if [ "${MINORVER}" -lt 5 ]; then
				EL_TE="syslog_ng.el6.0to4.te.in"
			else
				EL_TE="syslog_ng.el6.5up.te.in"
				    
				# 601/tcp and 601/udp are allowed by default on RHEL6.5+, so there is no need to enable them
				omit_allowed_ports
			fi
			;;
		7.*)
			EL_FC="syslog_ng.el78.fc.in"
			EL_TE="syslog_ng.el7.te.in"
			
			# 601/tcp and 601/udp are allowed by default on RHEL7, so there is no need to enable them
			omit_allowed_ports
			;;
		8.*)
			EL_FC="syslog_ng.el78.fc.in"
			EL_TE="syslog_ng.el8.te.in"

			# 601/tcp and 601/udp are allowed by default on RHEL8, so there is no need to enable them
			omit_allowed_ports
			;;
		*)
			echo "You don't seem to be running a supported version of RHEL!" >&2
			exit 250
			;;
	esac
}


substitute_install_path() {
	sed -e "s:^\\\$PATH\\\$:${INSTALL_PATH}:g" "src/root_unsafe/${EL_FC}"
	sed -e "s:^\\\$PATH\\\$:${INSTALL_PATH}:g" "src/root_safe/${EL_FC}"
}


omit_install_path() {
	sed -e "s:^\\\$PATH\\\$::g" "src/root_safe/${EL_FC}"
}


prepare_files() {
	echo "Using '${INSTALL_PATH}'..." 
	if [ "${INSTALL_PATH}" != "/" ]; then
		
		substitute_install_path > "syslog_ng.fc"
	else
		omit_install_path > "syslog_ng.fc"
	fi
	cat "src/syslog_ng.module.version" "src/${EL_TE}" > "syslog_ng.te"
}


remove_trainling_slash() {
	# the trailing slash in the install path (if present) breaks file context rules
	# thus it needs to be removed (provided that the install path is not "/" itself)
	sed -e 's:^\(.\+\)/$:\1:'
}


filter_bogus_build_output() {
	#filter misleading output caused by RHEL bug 1861968
	fgrep -v /usr/share/selinux/devel/include/services/container.if
}


build_module() {
	echo "Building and Loading Policy"
	build_output=$( make -f /usr/share/selinux/devel/Makefile syslog_ng.pp 2>&1 )
	retval=${?}
	filter_bogus_build_output <<<"${build_output}"
	[ ${retval} -eq 0 ] || exit 252
}


add_ports() {
	for entry in ${@}; do
		port=${entry%/*}
		proto=${entry#*/}
		semanage port -a -t syslogd_port_t -p ${proto} ${port} 2>/dev/null || \
		semanage port -m -t syslogd_port_t -p ${proto} ${port} 2>/dev/null
	done
}


install_module() {
	if /usr/sbin/semodule -l | grep -qw syslog_ng; then
		echo "The Syslog-ng SELinux policy module is already installed. Nothing to do..."
		echo "If it belongs to a previous version, then you will have to remove it first."
	else
		/usr/sbin/semodule -i syslog_ng.pp -v || exit 251

		# set up syslog-ng specific ports
		PORTS=
		for port in ${SYSLOG_NG_TCP_PORTS}; do PORTS="${PORTS} ${port}/tcp"; done
		for port in ${SYSLOG_NG_UDP_PORTS}; do PORTS="${PORTS} ${port}/udp"; done
		add_ports "${PORTS}"

		# Fixing the file context
		/sbin/restorecon -F -Rv "${INSTALL_PATH}"
		[ -f /etc/init.d/syslog-ng ] && /sbin/restorecon -F -v /etc/init.d/syslog-ng
		[ -f /etc/rc.d/init.d/syslog-ng ] && /sbin/restorecon -F -v /etc/rc.d/init.d/syslog-ng
		/sbin/restorecon -F -Rv /dev/log

		echo -e "\nInstallation of the Syslog-ng SELinux policy module finished.\nPlease restart syslog-ng. You can find more information about this in the README file."
	fi
}

remove_ports() {
	for entry in ${@}; do
		port=${entry%/*}
		proto=${entry#*/}
		semanage port -d -t syslogd_port_t -p ${proto} ${port} 2>/dev/null
	done
}

remove_module() {
	if /usr/sbin/semodule -l | grep -q syslog_ng; then
		echo -n "Removing Syslog-ng SELinux policy module... "
		
		/usr/sbin/semodule --remove=syslog_ng
		
		# unconfigure syslog-ng specific ports
		PORTS=
		for port in ${SYSLOG_NG_TCP_PORTS}; do PORTS="${PORTS} ${port}/tcp"; done
		for port in ${SYSLOG_NG_UDP_PORTS}; do PORTS="${PORTS} ${port}/udp"; done
		remove_ports "${PORTS}"
		
		[ -f syslog_ng.pp ] && rm -f syslog_ng.pp
		[ -f syslog_ng.te ] && rm -f syslog_ng.te
		[ -f syslog_ng.fc ] && rm -f syslog_ng.fc
		[ -f syslog_ng.if ] && rm -f syslog_ng.if
		[ -d tmp ] && rm -Rf tmp
		
		echo "done."
	else
		echo "No installed Syslog-ng SELinux policy module was found. No removal is necessary. Skipping..."
	fi
}

DIRNAME=$( dirname "${0}" )
cd "${DIRNAME}"
USAGE="Usage: $0\t[ --install-dir <DIRECTORY> | --remove | --help ]\n\n$0:\tA tool for building and managing the SELinux policy for the\n\t\tdefault syslog-ng installation."


while [ -n "${1}" ]; do
	case "${1}" in
		--help)
			# if --help is supplied, the help message will be printed independently of any other options being specified
			TASK_SELECTED="showhelp"
			break
			;;
		--install-dir)
			[ "${TASK_SELECTED}" = "remove" ] && echo -e "ERROR: Conflicting options!\n\n${USAGE}" >&2 && exit 249
			TASK_SELECTED="install"
			check_dir "${2}" || exit 253
			INPUT="${2}"
			shift 2
			;;
		--remove)
			[ "${TASK_SELECTED}" = "install" ] && echo -e "ERROR: Conflicting options!\n\n${USAGE}" >&2 && exit 249
			TASK_SELECTED="remove"
			shift
			;;
		*)
			echo -e "ERROR: Invalid option: '${1}'\n${USAGE}" >&2
			exit 255
			;;
	esac

done

case "${TASK_SELECTED}" in
	showhelp)
		echo -e "${USAGE}"
		exit 0
		;;
	remove)
		detect_os_version
		setup_vars
		remove_module
		exit 0
		;;
	install|install_default)
		if [ $( id -u ) != 0 ]; then
			echo 'You must be root to run this script!' >&2
			exit 254
		fi
		
		get_console_tty

		if [ -z "${INPUT}" ]; then 
			query_install_path
			while verify_input; do
				query_install_path
			done
		fi
		
		INSTALL_PATH=$( remove_trainling_slash <<<"${INPUT}" )
		
		detect_os_version
		install_precheck
		setup_vars
		prepare_files
		build_module
		install_module
		;;
	*)
		echo -e "ERROR: Invalid task: '${TASK_SELECTED}'!" >&2
		exit 248
		;;
esac
