package org.syslog_ng.elasticsearch_v2.messageprocessor.http;

@FunctionalInterface
public interface IndexFieldHandler<T> {
     T handle(String index, String type, String id, String pipeline, String formattedMessage);
}
