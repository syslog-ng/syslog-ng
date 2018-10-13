java destination
================

java destination gives an abstract class to implement destinations based on Java

If you want to implement a java destination, you should extend SyslogNgDestination abstract class
which is contained by the syslog-ng-core.jar which jar file will be in the moduledir after make install

Example implementation of a dummy destination

```
import org.syslog_ng.*;

public class TestClass extends TextLogDestination {

  public TestClass(long handle) {
    super(handle);
  }

  public boolean init()
  {
    System.out.println("START");
    System.out.println("Initialize test destination");
    return true;
  }

  public void deinit()
  {
    System.out.println("Deinitialize object");
  }

  public boolean queue(String message)
  {
    System.out.println("This is queue!" + message);
    return true;
  }

  public boolean flush()
  {
    return true;
  }
}
```

You need compile your written java based destination to create a .class or a .jar file
```
javac -cp /usr/lib/syslog-ng/syslog-ng-core.jar TestClass.java
```

You have to set the path of the class file or the name of the jar file (with full path) in the syslog-ng config

Example configuration for this (if you compiled the java file above, and the class file is in the /tmp directory):
```
@version: 3.18

options {
  threaded(yes);
};

source s_tcp{
  tcp(
    port(5555)
  );
};

destination d_java{
  java(
    class_name("TestClass")
    class_path("/tmp")
    option("key", "value")
  );
};

log{
  source(s_tcp);
  destination(d_java);
  flags(flow-control);
};

```
Example configuration using jar files in the class_path (the path inside the jar file has to been set using '.'):
```
@version: 3.18

options {
  threaded(yes);
};

source s_local {
        internal();
};

source s_network {
        tcp(port(5555));
};

destination d_local {
  java(
    class_path("/usr/lib/syslog-ng/3.6/elasticsearch.jar:/usr/share/elasticsearch/lib/elasticsearch-1.4.0.jar:/usr/share/elasticsearch/lib/lucene-core-4.10.2.jar")
    class_name("org.syslog_ng.destinations.ElasticSearch")
    template("$(format-json --scope rfc5424 --exclude DATE --key ISODATE)")
    option("cluster" "cl1")
    option("index" "syslog")
    option("type" "test")
    option("server" "192.168.1.104")
    option("port" "9300")
  );
};

log {
  source(s_network);
  destination(d_local);
  flags(flow-control);
};

```

Trouble shooting
----------------

### Set library path
If you want to use the syslog-ng java destination you have to add the path of the libjvm.so to the LD_LIBRARY_PATH
so if you get the following error, it means, that the LD_LIBRARY_PATH does not contain the path of libjvm and you have to set it:
```
Error opening plugin module; module='mod-java', error='libjvm.so: cannot open shared object file: No such file or directory'
```


