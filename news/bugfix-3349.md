syslog-ng-ctl: when syslog-ng gets stuck on executing a heavy stats-ctl command, should be
able to do a graceful shutdown when it is requested

The solution has two parts:
 1) move control loop to a separated thread
 2) run stats and query ctl commands in a separated thread


