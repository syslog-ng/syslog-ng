#!/bin/bash

set -e

SOURCE_DIR=/source
USER_NAME=${USER_NAME_ON_HOST:-dockerguest}
USER_ID=`stat -c '%u' $SOURCE_DIR`
GROUP_NAME=$USER_NAME
GROUP_ID=`stat -c '%g' $SOURCE_DIR`

if [[ "$USER_ID" -eq 0 ]]; then
    "$@"
else
    if ! getent passwd $USER_ID > /dev/null
    then
        groupadd --gid $GROUP_ID $GROUP_NAME &>/dev/null || \
            groupadd --gid $GROUP_ID dockerguest &>/dev/null || \
            echo "Failed to add group $GROUP_NAME/$GROUP_ID in docker entrypoint.sh";
        useradd $USER_NAME --uid=$USER_ID --gid=$GROUP_ID &>/dev/null || \
            useradd dockerguest --uid=$USER_ID --gid=$GROUP_ID &>/dev/null || \
            echo "Failed to add user $USER_NAME/$USER_ID in docker entrypoint.sh";
        mkdir -p /home/$USER_NAME
        chown $USER_NAME:$GROUP_ID /home/$USER_NAME
    fi
    chown ${USER_NAME} /source /build

    exec gosu "${USER_NAME}" "$@"
fi
