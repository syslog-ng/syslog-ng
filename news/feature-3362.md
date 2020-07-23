signal: use signal-cli via a program destination

Adding Signal (https://signal.org) destintaion to Syslog-ng,
relying on the work of https://github.com/AsamK/signal-cli

# The destination requires manual setup before use!
Please refer to official signal-cli documentation for
further information about registering it as a client:
https://github.com/AsamK/signal-cli#usage


# Example usage:

```
destination d_signal_cli{
  signal-cli-dest(
    signal-cli-executable("/usr/local/bin/signal-cli")
    own-number("+36001111111")
    partner-number("+36002222222")
  );
};
```


# WARNING! Performance measurements
Syslog-ng program destination assumes that the consumer has a
"daemon" like behaviour. The program will be started by Syslog-ng
at initialization time, and it will receive new messages on it's
standard input, separated by new line characters.

This aproach is very intentional from Syslog-ng. Since starting
and freeing up a process is a very resource intensive work,
doing it on per message basis can put a system on it's kneels.

In order to make signal-cli feasible we have to write a small
"wrapper" script, which achieves this "daemon like" behaviour.
Thus, this implementation is only intended to use for "alerts"
and "notifcations". Not for implementing heavy duty message
delivery pipelines.
