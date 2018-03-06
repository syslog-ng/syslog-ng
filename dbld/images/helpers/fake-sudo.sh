#!/bin/bash

CONTAINER_ID=$(cat /proc/self/cgroup | grep "docker" | cut -d'/' -f 3 | cut -c1-12 | sort -u | tail -1)
DOCKER_COMMAND="docker exec -it ${CONTAINER_ID} /bin/bash"

echo "sudo command is not available, to start a privileged shell inside this container, run the following command in a new terminal:"
echo "${DOCKER_COMMAND}"
