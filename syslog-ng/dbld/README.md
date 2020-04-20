## Syslog-ng development/release environment powered by Docker
With the help of the following tool you can
- compile syslog-ng from source code
- generate tarball
- create OS specific packages

in an isolated Docker container based environment.

## Usage information
```bash
~/syslog-ng$ dbld/rules [help]
```
dbld/rules is the general entrypoint for the tool, specifying multiple targets. For the complete list of the available targets please run the command (without parameters it will run without any side effect), or read the source code on [GitHub](rules).

Almost every `dbld/rules` command run in a Docker container. You can use the pre-built containers from [DockerHub](https://hub.docker.com/u/balabit/) or build your own images with the `dbld/rules image-<os>` command.

The source code and build products are mounted externally in the following locations:
- **/source** -> syslog-ng/*
- **/build** -> syslog-ng/dbld/build
- **/install** -> syslog-ng/dbld/install

## Example
### Building syslog-ng from source using Ubuntu Xenial (default)
> Assume that we have cloned syslog-ng's source into the `$HOME/syslog-ng` directory.

The following commands starts a container mounted with the source:

```bash
$ dbld/rules shell-ubuntu-xenial
```

You can also build a DEB using:

```bash
$ dbld/rules deb-ubuntu-xenial
```

You can find the resulting debs in `$HOME/syslog-ng/dbld/build`.

You can also use this image to hack on syslog-ng by configuring and building manually. Steps for the manual build after entering into the containers shell:

```bash
$ cd /source/
$ ./autogen.sh
$ cd build/
$ ../configure --enable-debug --prefix=/install
$ make
$ make check
$ make install
```

If the compilation and installation was successful you can run syslog-ng with the following command:

```bash
$ /install/syslog-ng/sbin/syslog-ng -Fedv
```

## sudo
The `sudo` command is not available inside this development container.

>Short explanation:
>
>Many of the image maintainers (mostly for security reasons) do not install the sudo package by default. Additionally (to avoid access problems to your repository outside of this container), we
>- mount directories
>- run commands
>
>inside this container using your external Username and ID.

There are many options to circumvent this limitation (i.e. Create your own image, based on this one.), but probably the easiest way is to start a new privileged shell in the already running container, using `docker exec`.
```bash
$ docker exec -it <container-name or ID> /bin/bash
```

> note: We installed a fake `sudo` command inside the container, which will print out a copy-paste ready version of the `docker exec` command in case someone accidentally calls it.
