/*
 * Copyright (c) 2015 BalaBit
 * All Rights Reserved.
 */

package org.syslog_ng.kafka;

import org.apache.kafka.clients.producer.Callback;
import org.apache.kafka.clients.producer.KafkaProducer;
import org.apache.kafka.clients.producer.ProducerRecord;
import org.apache.kafka.clients.producer.RecordMetadata;
import org.apache.log4j.Logger;
import org.syslog_ng.LogMessage;
import org.syslog_ng.StructuredLogDestination;
import org.syslog_ng.logging.SyslogNgInternalLogger;
import org.syslog_ng.options.InvalidOptionException;
import org.syslog_ng.kafka.KafkaDestinationOptions;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.Future;

public class KafkaDestination extends StructuredLogDestination {
  private KafkaProducer<String, String> producer;
  private KafkaDestinationProperties properties;
  private boolean isOpened = false;
  private Logger logger;
  private KafkaDestinationOptions options;

  public KafkaDestination(long handle) {
    super(handle);
    logger = Logger.getRootLogger();
    SyslogNgInternalLogger.register(logger);
    options = new KafkaDestinationOptions(this);
    properties = new KafkaDestinationProperties(options);
  }

    @Override
    public String getNameByUniqOptions() {
      return String.format("KafkaDestination,%s,%s", options.getKafkaBootstrapServers(), options.getTopic().getValue());
    }

  @Override
  public boolean send(LogMessage logMessage) {
    String formattedKey = options.getKey().getResolvedString(logMessage);
    String formattedTopic = options.getTopic().getResolvedString(logMessage);
    String formattedMessage = options.getTemplate().getResolvedString(logMessage);

    ProducerRecord<String,String> producerRecord = new ProducerRecord<String,String>(formattedTopic, formattedKey, formattedMessage);

    logger.debug("Outgoing message: " + producerRecord.toString());
    if (options.getSyncSend()) {
      return sendSynchronously(producerRecord);
    } else
      return sendAsynchronously(producerRecord);
  }

  private boolean sendSynchronously(ProducerRecord<String, String> producerRecord) {
    Future<RecordMetadata> future = producer.send(producerRecord);
    return waitForReceive(future);
  }

  private boolean sendAsynchronously(ProducerRecord<String, String> producerRecord) {
    Callback callback = new Callback() {
      public void onCompletion(RecordMetadata metadata, Exception e) {
        if(e != null) {
          logger.error(e.getMessage());
        }
      }
    };

    producer.send(producerRecord, callback);
    return true;
  }

  private boolean waitForReceive(Future<RecordMetadata> future) {
    try {
      future.get();
      return true;
    } catch (InterruptedException e) {
      logSendError(e);
    } catch (ExecutionException e) {
      logSendError(e);
    }

    return false;
  }

  private void logSendError(Exception e) {
    logger.debug("Unable to send the message at the moment");
    logger.debug(e.getMessage());
  }

  @Override
  public boolean open() {
    logger.debug("opening connection");
    producer = new KafkaProducer<String,String>(properties.getProperties());
    isOpened = true;
    return true;
  }

  @Override
  public void close() {
    logger.debug("closing connection");
    producer.close();
    isOpened = false;
  }

  @Override
  public boolean isOpened() {
    return isOpened;
  }

  @Override
  public boolean init() {
    logger.debug("initializing");
    try {
      options.init();
      properties.init();
      return true;
    } catch (InvalidOptionException e) {
      logger.error(e.getMessage());
      return false;
    }
  }

  @Override
  public void deinit() {
    logger.debug("deinitializing");
    options.deinit();
  }
}
