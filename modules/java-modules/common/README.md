# Kafka setup

* Download Kafka: [download link](https://www.apache.org/dyn/closer.cgi?path=/kafka/0.8.2.1/kafka-0.8.2.1-src.tgz)
* Extract the compressed file, step into the directory and run `gradle build`

# Start Kafka server

```
stentor@stentor-x201:~/work/kafka/kafka$ bin/zookeeper-server-start.sh config/zookeeper.properties
stentor@stentor-x201:~/work/kafka/kafka$ bin/kafka-server-start.sh config/server.properties
stentor@stentor-x201:~/work/kafka/kafka$ bin/kafka-topics.sh --create --zookeeper 127.0.0.1:2181 --replication-factor 1 --partitions 1 --topic mytopic
stentor@stentor-x201:~/work/kafka/kafka$ bin/kafka-console-consumer.sh --zookeeper localhost:2181 --topic mytopic --from-beginning
```

If you want to try it with the default test-producer:
```
bin/kafka-console-producer.sh --broker-list localhost:9092 --sync --topic mytopic
```

* build syslog-ng (from syslog-ng-core):

```
./sbuild.py make install
```

# Start syslog-ng

Sample config:

```
@version: 3.19
@include "scl.conf"
@module "mod-java"

source s_local {
#       system();
        internal();
};

source s_network {
        tcp(port(1514));
};

destination d_local {
        file("/tmp/messages");
};

destination d_java_0{
  java(
    class_name("org.syslog_ng.kafka.KafkaDestination")
    class_path("/home/btibi/work/syslog-ng-pe-project-5.4/install/lib/syslog-ng/java-modules/kafka.jar")
    option("kafka_bootstrap_servers", "127.0.0.1:9092")
    option("topic", "mytopic")
    option("key", "$HOST")
    option("sync_send", "false")
    option("template", "$MESSAGE")
    option("properties_file", "/home/btibi/work/syslog-ng-pe-project-5.4/install/etc/kafka.properties")
  );
};

log {
        source(s_local);
        source(s_network);
        destination(d_java_0);
        destination(d_local);
};

```

Properties file:

```
acks=all
metadata.fetch.timeout.ms=5000
retry.backoff.ms=1000
reconnect.backoff.ms=1000
```

Now you can use `loggen` to send messages into `syslog-ng`:

```
loggen -S -n 10 localhost 1514
```

## Other infos about gradle
[link](http://gitlab.syslog-ng.balabit/lbudai/internal-builder-repo-doc/wikis/home)
