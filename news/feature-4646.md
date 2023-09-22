`--check-startup`: a new command line option for syslog-ng along with the
existing `--syntax-only`. This new option will do a complete configuration
initialization and then exit with exit code indicating the result. Since
this also initializes things like network listeners, it will probably _not_
work when there is another syslog-ng instance running in the background. The
recommended use of this option is a dedicated config check container, as
explained in #4592.
