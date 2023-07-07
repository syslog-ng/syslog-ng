`parallelize()` support for pipelines: syslog-ng has traditionally performed
processing of log messages arriving from a single connection sequentially.
This was done to ensure message ordering as well as most efficient use of
CPU on a per message basis.  This mode of operation is performing well as
long as we have a relatively large number of parallel connections, in which
case syslog-ng would use all the CPU cores available in the system.

In case only a small number of connections deliver a large number of
messages, this behaviour may become a bottleneck.

With the new parallelization feature, syslog-ng gained the ability to
re-partition a stream of incoming messages into a set of partitions, each of
which is to be processed by multiple threads in parallel.  This does away
with ordering guarantees and adds an extra per-message overhead. In exchange
it will be able to scale the incoming load to all CPUs in the system, even
if coming from a single, chatty sender.

To enable this mode of execution, use the new parallelize() element in your
log path:

```
log {
	source {
		tcp(
			port(2000)
			log-iw-size(10M) max-connections(10) log-fetch-limit(100000)
		);
	};
	parallelize(partitions(4));

	# from this part on, messages are processed in parallel even if
	# messages are originally coming from a single connection

	parser { ... };
	destination { ... };
};
```

The config above will take all messages emitted by the tcp() source and push
the work to 4 parallel threads of execution, regardless of how many
connections were in use to deliver the stream of messages to the tcp()
driver.

parallelize() uses round-robin to allocate messages to partitions by default.
You can however retain ordering for a subset of messages with the
partition-key() option.

You can use partition-key() to specify a message template. Messages that
expand to the same value are guaranteed to be mapped to the same partition.

For example:

```
log {
	source {
		tcp(
			port(2000)
			log-iw-size(10M) max-connections(10) log-fetch-limit(100000)
		);
	};
	parallelize(partitions(4) partition-key("$HOST"));

	# from this part on, messages are processed in parallel if their
	# $HOST value differs. Messages with the same $HOST will be mapped
	# to the same partition and are processed sequentially.

	parser { ... };
	destination { ... };
};
```

NOTE: parallelize() requires a patched version of libivykis that contains
this PR https://github.com/buytenh/ivykis/pull/25.  syslog-ng source
releases bundle this version of ivykis in their source trees, so if you are
building from source, be sure to use the internal version
(--with-ivykis=internal).  You can also use Axoflow's cloud native container
image for syslog-ng, named AxoSyslog
(https://github.com/axoflow/axosyslog-docker) which also incorporates this
change.
