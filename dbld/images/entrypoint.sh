#!/bin/bash

set -e

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
    if getent group sudo >/dev/null 2>&1; then
        usermod -a -G sudo "$USER_NAME"
    else
        echo "No sudo group on this system, trying wheel!";
        if getent group wheel >/dev/null 2>&1; then
            usermod -a -G wheel "$USER_NAME"
        else
            echo "No sudo or wheel group found, configuring sudoers directly"
            echo "$USER_NAME ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/$USER_NAME
            chmod 440 /etc/sudoers.d/$USER_NAME
        fi
    fi
    if [ -f /etc/sudoers ]; then
        sed -i -e '/^%sudo\s\+ALL=/s,ALL$,NOPASSWD: ALL,' /etc/sudoers
        sed -i -e '/^%wheel\s\+ALL=/s,ALL$,NOPASSWD: ALL,' /etc/sudoers
    fi
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
    echo "Added new user: $USER_NAME"
    exec sudo --preserve-env --preserve-env=PATH -Hu "${USER_NAME}" "$@"
fi
