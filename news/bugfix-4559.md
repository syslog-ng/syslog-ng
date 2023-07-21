`network()`,`syslog()`,`tcp()` destination: fix TCP keepalive

`tcp-keepalive-*()` options were broken on the destination side since v3.34.1.
