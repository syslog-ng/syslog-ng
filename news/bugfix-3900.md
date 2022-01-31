`syslog-ng`: fix a SIGSEGV triggered by an incorrectly formatted "CONFIG"
command, received on the syslog-ng control socket.  The only known
implementation of the control protocol is syslog-ng-ctl itself, which always
sends a correct command, but anyone with access to the UNIX domain socket
`syslog-ng.ctl` (root only by default) can trigger a crash.
