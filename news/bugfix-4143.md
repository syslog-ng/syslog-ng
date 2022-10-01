`syslog-ng --help` screen: the output for the --help command line option has
included sample paths to various files that contained autoconf style
directory references (e.g. ${prefix}/etc for instance). This is now fixed,
these paths will contain the expanded path. Fixes Debian Bug report #962839:
https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=962839
