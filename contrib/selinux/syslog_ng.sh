#!/bin/sh -e

DIRNAME=`dirname $0`
cd $DIRNAME
USAGE="$0 [ --update ]"
if [ `id -u` != 0 ]; then
	echo 'You must be root to run this script'
	exit 1
fi

if [ ! -f /etc/selinux/config ]; then
	echo 'SELinux not installed on this machine.'
	exit 1
fi

. /etc/selinux/config

if [ -z "$SELINUX" ] || [ "$SELINUX" = "disabled" ]; then
	echo 'SELinux not configured.'
	exit 1
fi

if [ ! -f /usr/share/selinux/devel/Makefile ]; then
	echo 'selinux-policy package missing. please install it.'
	exit 1
fi

if [ $# -eq 1 ]; then
	if [ "$1" = "--update" ] ; then
		time=`ls -l --time-style="+%x %X" syslog_ng.te | awk '{ printf "%s %s", $6, $7 }'`
		rules=`ausearch --start $time -m avc --raw -se syslog_ng`
		if [ x"$rules" != "x" ] ; then
			echo "Found avc's to update policy with"
			echo -e "$rules" | audit2allow -R
			echo "Do you want these changes added to policy [y/n]?"
			read ANS
			if [ "$ANS" = "y" -o "$ANS" = "Y" ] ; then
				echo "Updating policy"
				echo -e "$rules" | audit2allow -R >> syslog_ng.te
				# Fall though and rebuild policy
			else
				exit 0
			fi
		else
			echo "No new avcs found"
			exit 0
		fi
	else
		echo -e $USAGE
		exit 1
	fi
elif [ $# -ge 2 ] ; then
	echo -e $USAGE
	exit 1
fi


echo "Building and Loading Policy"
make -f /usr/share/selinux/devel/Makefile
/usr/sbin/semodule -i syslog_ng.pp

# Fixing the file context on /opt/syslog-ng/sbin/syslog-ng
/sbin/restorecon -F -R -v /opt/syslog-ng/sbin/syslog-ng
