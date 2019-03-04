# Images
Resources for creating the development/release environment, designed to work both locally and with the DockerHub automated builds.

# Dockerfiles
Since Dockerfiles do not support the "include" mechanism (More on the topic [here](https://github.com/moby/moby/issues/735)), and environment variables are not supported on DockerHub (which is used for automatic builds) the following Dockerfiles look very much alike, following this scheme:

### FROM
We are starting from the official base images of the distributions.

### ENV
All platform specific parts are coded into variables.

### copy helpers
Copy every helper script/resource into the containers.

### package installation
In this section we will install all the dependencies we need, in the following order:

| Function | Reference | Description |
| - | - | - |
| install_apt_packages | [apt-get](https://en.wikipedia.org/wiki/APT_(Debian)) | See [packages.manifest](/dbld/images/helpers/packages.manifest) |
| install_pip_packages | [python -m pip](https://packaging.python.org/tutorials/installing-packages/) | See [pip_packages.manifest](/dbld/images/helpers/pip_packages.manifest) |
| install_criterion | [Criterion](https://github.com/Snaipe/Criterion) | We use criterion for unit testing. |
| install_gradle | [Gradle](https://gradle.org/) | We use gradle to build Java based drivers. |
| install_gosu \<platform\> | [Gosu](https://github.com/tianon/gosu) | We use gosu to drop root privileges inside the containers. |
> Note: These functions can be found in [helpers/dependencies.sh](/dbld/images/helpers/dependencies.sh)

### fake sudo
Just a simple `echo` command to make life easier. Reasons are explained in details in [/dbld/README.md](/dbld/README.md)

### volumes
Creating the `/source` and `/build` folders for our work. (note: `install` folder will be placed inside the build folder)

### entrypoint
Custom entrypoint script will create the user account inside the container, so you will not have any access problem with the mounted folders.

# Package lists
There are some helper functions to install special packages, but the main source of the dependencies are the `*.manifest` files. Lines started with # are comments, and will be ignored. The platform and/or distribution names inside [] brackets are indicating on which platforms the package will be installed.
