#!/bin/sh
#
# $Id: syslog-ng.init.d,v 1.1.2.2 2006/03/02 18:35:45 folti Exp $
#
# adapted to syslog-ng by BJ, Aug, 7th 2000
# cleaned up by Bazsi, Oct, 12th 2000
# minor fix by Bojan Zdrnja, Apr, 11th 2003
#   added nicer options field
# Modified for BalaBit Ltd's syslog-ng package by Tamas Pal Mar, 1st 2006
#

DAEMON=/opt/syslog-ng/sbin/syslog-ng
CONFFILE=/opt/syslog-ng/etc/syslog-ng/syslog-ng.conf
OPTIONS="-f $CONFFILE"
PIDFILE="/var/run/syslog-ng.pid"
LD_LIBRARY_PATH=

case "$1" in
	start)
		if [ -f $PIDFILE ];then
		    echo "syslog-ng already running."
		    exit 1
		fi
	        if [ -f $CONFFILE -a -x $DAEMON ]; then
	                echo 'syslog-ng service starting.'
	                #
	                # Before syslog-ng starts, save any messages from previous
	                # crash dumps so that messages appear in chronological order.
	                #
	                /usr/bin/savecore -m
	                if [ -r /etc/dumpadm.conf ]; then
	                        . /etc/dumpadm.conf
	                        [ "x$DUMPADM_DEVICE" != xswap ] && \
	                            /usr/bin/savecore -m -f $DUMPADM_DEVICE
	                fi
	                $DAEMON $OPTIONS -p $PIDFILE
	        fi
        	;;

	stop)
	        if [ -f $PIDFILE ]; then
	                syspid=`/usr/bin/cat $PIDFILE`
	                [ "$syspid" -gt 0 ] && kill -15 $syspid && rm -f $PIDFILE
	        fi
        	;;
	reload)
		if [ -f $PIDFILE ]; then
	                syspid=`/usr/bin/cat $PIDFILE`
			
	                [ "$syspid" -gt 0 ] && kill -1 $syspid && echo "syslog-ng service reloaded"
	        fi
        	;;
	restart)
		$0 stop
		sleep 1
		$0 start 
		;;
	*)
	        echo "Usage: $0 { start | stop | restart | reload }"
	        exit 1
        	;;
esac
