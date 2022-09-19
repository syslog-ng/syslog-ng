`log-level()`: added a new global option to control syslog-ng's own internal
log level.  This augments the existing support for doing the same via the
command line (via -d, -v and -t options) and via syslog-ng-ctl.  This change
also causes higher log-levels to include messages from lower log-levels,
e.g.  "trace" also implies "debug" and "verbose".  By adding this capability
to the configuration, it becomes easier to control logging in containerized
environments where changing command line options is more challenging.

`syslog-ng-ctl log-level`: this new subcommand in syslog-ng-ctl allows
setting the log level in a more intuitive way, compared to the existing
`syslog-ng-ctl verbose|debug|trace -s` syntax.

`syslog-ng --log-level`: this new command line option for the syslog-ng
main binary allows you to set the desired log-level similar to how you
can control it from the configuration or through `syslog-ng-ctl`.
