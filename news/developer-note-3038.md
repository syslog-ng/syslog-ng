`dbld`: Added option to customize `shell` command.

Discussed in #2982, one can loose important information with
the automatic rm option. (i.e.: during an accidental reboot)
However removing the rm option by default, will increase the
chance of working with VERY old Docker images.

With this change, it is possible to override the option with
`rules.conf`, while keeping the default behaviour.

The simplest example: use existing images, start a new one if
there is none. (use docker rm manually if you want to update)

```
DOCKER_SHELL=$(DOCKER) inspect $* > /dev/null 2>&1; \
  if [ $$? -eq 0 ]; then \
    $(DOCKER) start -ia $*; \
  else \
    $(DOCKER) run $(DOCKER_RUN_ARGS) -ti --name $* balabit/syslog-ng-$* /source/dbld/shell; \
  fi
```
