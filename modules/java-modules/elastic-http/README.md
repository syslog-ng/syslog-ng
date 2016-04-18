ElasticSearch HTTP Destination
==============================

This is a simple ElasticSearch HTTP destination using the Jest library. Although it support bulk insert (flush_limit), unfortunately it doesn't support flush timeout.

Example configuration:
```
destination d_test {
  java(
    class_path("`module-path`/java-modules/*.jar:/install/jest-jars/*.jar")
    class_name("org.syslog_ng.elasticsearch_http.ElasticSearchHTTPDestination")
    option("index", "logs-$YEAR.$MONTH.$DAY")
    option("message_template", "$(format-json --scope rfc5424 --exclude DATE --key ISODATE --pair @timestamp=$ISODATE)")
    option("type", "logs")
    option("cluster_url", "http://localhost:9200")
    option("flush_limit", "100")
  );
};

```
