# Images
Resources for creating the development/release environment. This folder contains the
 - ***.dockerfile**s to build the distributions
 - **helpers** like entypoints and smaller subrutines
 - **required-*** lists of packages, grouped by the package managers/sources

# Dockerfiles
Since Dockerfiles do not support "include" mechanism (More on the topic [here](https://github.com/moby/moby/issues/735)), and environment variables are not supported on DockerHub (which is used for automatic builds) the following Dockerfiles look very much alike, following this scheme:

### FROM
We are starting from the official base images of the distributions.

### ARG
All platform specific parts are coded into variables.

### copy helpers
Copy every helper scripts/resources into the containers.

### pip and wget
Adding the pip package handler and wget to grab the resources like custom repository keys.

### package installation
In this section we will install all the packages we need, in the following order:
 - pip packages
 - official repository
 - custom repository

### gosu
gosu is a wonderfull project made for dropping root privileges easily. Check out [here](https://github.com/tianon/gosu).

### fake sudo
Just a simple "echo" script to make life easier. Reasons explained in details in [/dbld/README.md](/dbld/README.md)

### volumes
Creating the /source and /build folders for our work. (note: install folder will be placed inside the build folder)

### entrypoint
Custom entrypoint script will create the user account inside the container, so you will not have any access problem with the mounted folders.

### workarounds
Custom tweaks for temporary problems. The first problem - the reason we introduced this section - was with Docker Hub builds.


# Package lists
To avoid duplications, we tried to group together the list of the required packages between the different distributions. Luckily, they mostly have the same name with the same package managers, accross the different platforms.
Unfortunately, not every requirement can be satisfied with the official packages, so we have to build some dependencies from source. We are trying to eliminate those, and for the clarification they are listed in separate folders.

| Folder            | Function |
| ----------------- | -------- |
| required-**pip**  | List of **pip** packages (Currently the same for each platform) |
| required-**apt**  | List of official packages for **Ubuntu** and **Debian** |
| required-**obs**  | List of custom built packages for **Ubuntu** and **Debian** |
| required-**yum**  | List of official packages for **CentOS** |
| required-**epel** | List of custom built packages for **CentOS** |

### all.txt
Contains a list of packages which should be installed on each and every distribution with the appropiate package manager.

### ${DISTRO}*.txt
Platform specific list of packages. If there is no such file for your platform yet:
 - Add a new file, starting with the same name as your platform.
 - Make sure there is an empty line at the end of the file, because they are concatenated together.
 - Rebuild you image with the dbld/rules command.
