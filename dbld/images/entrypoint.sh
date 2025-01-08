#!/bin/bash

set -ex

SOURCE_DIR=/source
USER_NAME=${USER_NAME_ON_HOST:-dockerguest}
USER_ID=`stat -c '%u' $SOURCE_DIR`
GROUP_NAME=$USER_NAME
GROUP_ID=`stat -c '%g' $SOURCE_DIR`

function create_user() {
    groupadd --gid $GROUP_ID $GROUP_NAME &>/dev/null || \
        groupadd --gid $GROUP_ID dockerguest &>/dev/null || \
        echo "Failed to add group $GROUP_NAME/$GROUP_ID in docker entrypoint-debian.sh";
    useradd $USER_NAME --uid=$USER_ID --gid=$GROUP_ID &>/dev/null || \
        useradd dockerguest --uid=$USER_ID --gid=$GROUP_ID &>/dev/null || \
        echo "Failed to add user $USER_NAME/$USER_ID in docker entrypoint-debian.sh";
    usermod -a -G sudo $USER_NAME || usermod -a -G wheel $USER_NAME
    sed -i -e '/^%sudo\s\+ALL=/s,ALL$,NOPASSWD: ALL,' /etc/sudoers
    sed -i -e '/^%wheel\s\+ALL=/s,ALL$,NOPASSWD: ALL,' /etc/sudoers
    mkdir -p /home/$USER_NAME
    chown $USER_NAME:$GROUP_ID /home/$USER_NAME
}

if [[ "$USER_ID" -eq 0 ]]; then
    "$@"
else
    if getent passwd $USER_ID > /dev/null
    then
        echo "USER_ID: $USER_ID already exist in passwd database, performing cleanup"
        userdel --remove $(getent passwd $USER_ID | cut -d":" -f1)
        create_user
    else
        create_user
    fi
	ls -l /etc/sudoers
	ls -l /etc/pam.d
	echo =====
	cat /etc/pam.d/sudo
	echo =====
	cat /etc/shadow
	echo =====
	cat /etc/gshadow
    id $USER_NAME
    echo "Added new user: $USER_NAME"
    exec sudo --preserve-env --preserve-env=PATH -Hu "${USER_NAME}" "$@"
fi
