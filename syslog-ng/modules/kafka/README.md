Kafka destination
=================

Here is a simple configuration sending the messages on a dedicated
Kafka queue (`syslog-ng`) using Logstash's JSON event layout:

```
source s_system {
  system();
};

destination d_kafka {
  channel {
    rewrite {
      set("${HOST}"    value(".eventv1.host"));
      set("1"          value(".eventv1.@version"));
      set("${ISODATE}" value(".eventv1.@timestamp") condition("${.eventv1.@timestamp}" eq ""));
      set("${MESSAGE}" value(".eventv1.message")    condition("${.eventv1.message}" eq ""));
      set("${MSG}"     value(".eventv1.message")    condition("${.eventv1.message}" eq ""));
      set("generic"    value(".eventv1.type")       condition("${.eventv1.type}" eq ""));
    };
    destination {
      kafka-c(properties(metadata.broker.list("localhost:9092")
                       queue.buffering.max.ms("1000"))
            topic("syslog-ng")
            payload("$(format-json --key .eventv1.* --rekey .eventv1.* --shift 9)"));
    };
  };
};

log {
  source(s_system);
  destination(d_kafka);
};
```

Compilation
-----------

You need [librdkafka](https://github.com/edenhill/librdkafka/). Once
installed, compile with `--with-librdkafka`.

Running Kafka
-------------

If you are not too familiar with Kafka, a simple recipe using
[Docker](http://docker.io) can get you started in a minute:

```
git clone https://github.com/spotify/docker-kafka
docker build -t spotify/kafka docker-kafka/kafka
docker run -e HELIOS_PORT_kafka=vcap.me:9092 -p 2181:2181 -p 9092:9092 spotify/kafka
```

Another useful tool is
[kafkacat](https://github.com/edenhill/kafkacat). For example, to look
at logs sent to Kafka, use:

```
kafkacat -C -u -b localhost -t syslog-ng
```
