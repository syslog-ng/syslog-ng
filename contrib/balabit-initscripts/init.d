#!/bin/sh

# syslog-ng	This starts and stops syslog-ng
#
# chkconfig:    - 12 88
# processname: /opt/syslog-ng/sbin/syslog-ng
# config:      /opt/syslog-ng/etc/syslog-ng.conf
# config:      /etc/sysconfig/syslog-ng
# pidfile:     /var/run/syslog-ng.pid

### BEGIN INIT INFO
# Provides: syslog
# Description: reads and logs messages to the system console, log \
#              files, other machines and/or users as specified by \
#              its configuration file.
# Short-Description: Next-generation syslog server. Proprietary edition.
# Required-Start: $local_fs
# Required-Stop:  $local_fs
# Default-Start:  2 3 4 5
# Default-Stop: 0 1 6
### END INIT INFO

OS=`uname -s`

DEFAULTFILE=/etc/default/syslog-ng
[ -f ${DEFAULTFILE} ] && . ${DEFAULTFILE}

SYSLOGNG_PREFIX=${SYSLOGNG_PE_PREFIX:-/opt/syslog-ng}
export SYSLOGNG_PREFIX
SYSLOGNG="$SYSLOGNG_PREFIX/sbin/syslog-ng"
CONFFILE=$SYSLOGNG_PREFIX/etc/syslog-ng.conf
PIDFILE=$SYSLOGNG_PREFIX/var/run/syslog-ng.pid
SYSLOGPIDFILE="/var/run/syslog.pid"

SLNG_INIT_FUNCTIONS=$SYSLOGNG_PREFIX/lib/init-functions
MAXWAIT=30

# for RedHat...
SUBSYSDIR=/var/lock/subsys

retval=0

# in HPUX, PATH for init scripts does not contain standard dirs for binaries
if [ "$OS" = "HP-UX" ]; then
   PATH=$PATH:/bin:/usr/bin:/sbin:/usr/sbin
fi


# OS specific I-didn't-do-anything exit function.
exit_noop() {
	case $OS in
		HP-UX) exit 2
			;;
		*)     exit 0
			;;
	esac
}

# Platform specific echo -n implementation.
echo_n() {
	case "$OS" in
		SunOS) echo "$@\c "
			;;
		HP-UX) echo "  $@\c "
			;;
		*)     echo -n "$@"
			;;
		esac
}

test -x ${SYSLOGNG} || exit_noop
test -r ${CONFFILE} || exit_noop

if [ -f /etc/lsb-release ]; then
	. /etc/lsb-release
fi

if [ -f "/etc/redhat-release" ];then
	# redhat uses a different syslogd pidfile...
	SYSLOGPIDFILE="/var/run/syslogd.pid"
fi

if [ "$OS" = "SunOS" ] || [ "$OS" = "Solaris" ];then
	if [ "`uname -r`" = "5.8" ];then
		SYSLOGPIDFILE="/etc/syslog.pid"
	fi
fi

. $SLNG_INIT_FUNCTIONS

case "$OS" in
	Linux)
		SYSLOGNG_OPTIONS="--no-caps"
		;;
esac

#we source /etc/default/syslog-ng if exists
[ -r /etc/default/syslog-ng ] && . /etc/default/syslog-ng

# Source config on RPM systems
[ -r /etc/sysconfig/syslog-ng ] && . /etc/sysconfig/syslog-ng

#we source /etc/default/syslog-ng if exists
[ -r $SYSLOGNG_PREFIX/etc/default/syslog-ng ] && \
	. $SYSLOGNG_PREFIX/etc/default/syslog-ng

create_xconsole() {
	if grep -v '^#' $CONFFILE | grep /dev/xconsole >/dev/null 2>&1; then
		if [ ! -e /dev/xconsole ]; then
			mknod -m 640 /dev/xconsole p
		fi
	fi
}

check_syntax() {
	${SYSLOGNG} --no-caps --syntax-only
	_rval=$?
	[ $_rval -eq 0 ] || exit $_rval
}

slng_waitforpid() {
	_pid=$1
	_process=$2
	_cnt=$MAXWAIT

	# no pid, return...
	[ -z "$_pid" ] && return 0

	_procname=`basename $_process`

	while [ $_cnt -gt 0 ]; do
		_numproc=`ps -p $_pid $PSOPTS | grep $_procname | wc -l`
		if [ $_numproc -eq 0 ]; then
			return 0
		fi
		_cnt=`expr $_cnt - 1`
		sleep 1
	done
	return 1
}

returnmessage() {
	_rval=$1
        is_debian=0
        case "$OS" in
            Linux)
               res=`cat /etc/issue | grep Debian`
               is_debian=$?
            ;;
        esac
	if [ $_rval -ne 0 ];then
                if [ $is_debian -eq 0 ];then
			log_failure_msg "failed"
		else
			log_failure_msg
		fi
	else
		if [ $is_debian -eq 0 ];then
			log_success_msg "OK"
		else
			log_success_msg
		fi
	fi
}

syslogng_start() {
	echo_n "Starting syslog-ng: "
	PID=`pidofproc -p ${PIDFILE} ${SYSLOGNG} | head -1`
	if [ -n "$PID" ] && [ $PID -gt 0 ] ;then
		log_success_msg "already running: $PID"
		return $retval
	fi
	rm -f ${PIDFILE}
	start_daemon -f -p ${PIDFILE} ${SYSLOGNG} ${SYSLOGNG_OPTIONS}
	retval=$?
	returnmessage $retval
	if [ $retval -eq 0 ];then
		if [ "$OS" = "Linux" ] && [ -d $SUBSYSDIR ];then
			touch $SUBSYSDIR/syslog-ng
		fi
		# remove symlinks
		if [ -h $SYSLOGPIDFILE ];then
			rm -f $SYSLOGPIDFILE
		fi
		if [ ! -f $SYSLOGPIDFILE ];then
			ln -s $PIDFILE $SYSLOGPIDFILE
		fi
	fi
	return $retval
}

# Can't name the function stop, because HP-UX has a default alias named stop...
syslogng_stop() {
	echo_n "Stopping syslog-ng: "
	PID=`pidofproc -p ${PIDFILE} ${SYSLOGNG} | head -1`
	retval=$?
	if [ $retval -ne 0 ] || [ -z "$PID" ];then
		return $retval
	fi
	killproc -p ${PIDFILE} ${SYSLOGNG}
	retval=$?
	if [ $retval -eq 0 ];then
		slng_waitforpid "$PID" ${SYSLOGNG}
		if [ $? -ne 0 ]; then
			retval=$?
			log_failure_msg " Timeout"
			killproc -p ${PIDFILE} ${SYSLOGNG} -KILL
		else
			returnmessage $retval
		fi
	fi
	if [ $retval -eq 0 ];then
		rm -f $SUBSYSDIR/syslog-ng ${PIDFILE}
		if [ -h $SYSLOGPIDFILE ];then
			rm -f $SYSLOGPIDFILE
		fi
	fi
	return $retval
}

syslogng_restart() {
	check_syntax
	echo_n "Restarting syslog-ng: "
	syslogng_stop
	syslogng_start
	return $retval
}

syslogng_reload() {
	echo_n "Reloading syslog-ng's config file: "
	check_syntax
	killproc -p ${PIDFILE} ${SYSLOGNG} -HUP
	retval=$?
	returnmessage $retval
	return $retval
}

syslogng_status() {
	echo_n "Checking for syslog-ng service: "
	pid=`pidofproc -p ${PIDFILE} ${SYSLOGNG}`
	retval=$?
	if [ $retval -ne 0 ];then
		msg=
		case $retval in
			1) msg="dead but pidfile exists." ;;
			*) msg="stopped" ;;
		esac
		log_failure_msg "$msg"
	else
		log_success_msg "$pid running"
	fi
	return $retval
}

syslogng_probe() {
	if [ ${CONFFILE} -nt ${PIDFILE} ]; then
		echo reload
	fi
}

case $OS in
	Linux)
		case "x$CONSOLE_LOG_LEVEL" in
			x[1-8])
				dmesg -n $CONSOLE_LOG_LEVEL
				;;
			x)
				;;
			*)
				echo "CONSOLE_LOG_LEVEL is of unaccepted value."
				;;
		esac
		create_xconsole
		;;
	SunOS)
			#
			# Before syslog-ng starts, save any messages from previous
			# crash dumps so that messages appear in chronological order.
			#
                        if [ -r /dev/dump ]; then
                                /usr/bin/savecore -m
                        fi
			if [ -r /etc/dumpadm.conf ]; then
				. /etc/dumpadm.conf
                                if [ -n "$DUMPADM_DEVICE" ] && [ -r "$DUMPADM_DEVICE" ] && \
                                   [ "x$DUMPADM_DEVICE" != xswap ]; then
                                        /usr/bin/savecore -m -f $DUMPADM_DEVICE
                                fi
			fi
		;;
	HP-UX)
		# /etc/default/syslog-ng, the HP-UX way.
		[ -r /etc/rc.config.d/syslog-ng ] && . /etc/rc.config.d/syslog-ng
		;;
esac


case "$1" in
	start_msg)
		echo "Start syslog-ng system message logging daemon"
		;;
	stop_msg)
		echo "Stop syslog-ng system message logging daemon"
		;;
	start|stop|restart|reload|probe)
		syslogng_$1
		;;
	force-reload)
		syslogng_restart
		;;
	reload-or-restart)
		PID=`pidofproc -p ${PIDFILE} ${SYSLOGNG} | head -1`
		if [ -n "$PID" ] && [ $PID -gt 0 ] ;then
			syslogng_reload
		else
			syslogng_start
		fi
		;;
	status)
		syslogng_status
		;;
	condrestart|try-restart)
		[ ! -f $lockfile ] || syslogng_restart
		;;
	rotate)
		syslogng_reload
		;;
	*)
		echo "Usage: $0 {start|stop|status|restart|try-restart|reload|force-reload|probe}"
		exit 2
		;;
esac

exit $retval

# vim: ts=2 ft=sh noexpandtab
