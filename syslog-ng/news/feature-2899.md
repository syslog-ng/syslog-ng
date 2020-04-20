`DESTIP/DESTPORT/PROTO`: new macros

These new macros express the destination ip, destination port and used protocol on a **source**.

The use-case behind the PR is as follows:
* someone has an appliance which sends out log messages via both UDP and TCP
* the format of the two are different, and he wants to capture either with the simplest possible filter
* netmask() doesn't work because the IP addresses are the same
* host() doesn't work because the hostnames are the same

Example:

```
log {
  source { network(localip(10.12.15.215) port(5555) transport(udp)); };
  destination { file("/dev/stdout" template("destip=$DESTIP destport=$DESTPORT proto=$PROTO\n")); };
};
```

Outut:

```
destip=10.12.15.215 destport=5555 proto=17
```
