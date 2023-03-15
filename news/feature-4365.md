`ebpf()` plugin and reuseport packet randomizer: a new ebpf() plugin was
added as a framework to leverage the kernel's eBPF infrastructure to improve
performance and scalability of syslog-ng.  The first eBPF based solution in
this framework improves performance when a single (or very few) senders
generate most of the inbound UDP traffic that syslog-ng needs to process.
Normally, the kernel distributes the load between so-reuseport sockets by
keeping each flow (e.g.  same source/dest ip/port) in its dedicated
receiver.  This fails to balance the sockets properly if only a few senders
are responsible for most of the load.  ebpf(reuseport()) will replace the
original kernel algorithm with an alternative: individual packets will be
assigned to one of the sockets in the group randomly, thereby producing a
more uniform load.

Example:

```
source s_udp {
        udp(so-reuseport(yes) port(2000) persist-name("udp1")
                ebpf(reuseport(sockets(4)))
        );
        udp(so-reuseport(yes) port(2000) persist-name("udp2"));
        udp(so-reuseport(yes) port(2000) persist-name("udp3"));
        udp(so-reuseport(yes) port(2000) persist-name("udp4"));
};
```

NOTE: The `ebpf()` plugin is considered advanced usage so its compilation is
disabled by default.  Please don't use it unless all other avenues of
configuration solutions are already tried.  You will need a special
toolchain and a recent kernel version to compile and run eBPF programs.  To
compile `ebpf()` you will need libbpf development package, bpftool (shipped as
part of the kernel) and `clang` with bpf target support.  To run `ebpf()` programs,
syslog-ng will require root privileges and a minimum kernel version of 5.5.
