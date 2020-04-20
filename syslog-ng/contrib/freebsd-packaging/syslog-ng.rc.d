#!/bin/sh

# startup script for FreeBSD 6.x.
# Written by Tamas Pal <folti@balabit.hu>

# PROVIDE: syslogd
# REQUIRE: mountcritremote cleanvar newsyslog
# BEFORE: SERVERS

# Enabling syslog-ng:
# * copy this script to /etc/rc.d or /usr/local/etc/rc.d
# * Edit /etc/rc.conf and add the following lines:
#   ----- CUT HERE ----- 
# syslogd_enable="NO"
# syslogng_enable="YES"
# syslogng_flags=""
#   ----- CUT HERE ----- 
# * Add your extra flags to syslogng_flags EXCEPT the -p <pidfile> and -f
#   <conffile> flags. These are added automatically by this script. To set
#   their value, change the value of the conf_file and pidfile variables below

. "/etc/rc.subr"

name="syslogng"

rcvar=`set_rcvar`
pidfile="/var/run/syslog-ng.pid"

## CONFIG OPTIONS: START
# modify them according to your environment.
installdir="/opt/syslog-ng"
localstatedir="${installdir}/var"
libpath="${installdir}/lib"
conf_file="${installdir}/etc/syslog-ng.conf"
## CONFIG OPTIONS: END

command="${installdir}/sbin/syslog-ng"
required_files="$conf_file"

start_precmd="syslog_ng_precmd"
extra_commands="reload"

LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$libpath
export LD_LIBRARY_PATH

syslog_ng_precmd() {
	if [ ! -d ${localstatedir} ]; then
		mkdir -p ${localstatedir}
	fi

	if [ ! -L /dev/log ]; then
		ln -s /var/run/log /dev/log
	fi
}

load_rc_config "$name"

case $1 in
	start)
		syslogng_flags=" -p ${pidfile} -f ${conf_file} ${syslog_ng_flags}"
	;;
esac

run_rc_command "$1"

# vim: ft=sh 
