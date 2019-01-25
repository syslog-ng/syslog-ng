/*
 * Copyright (c) 2015 Balabit
 * Copyright (c) 2015 Tibor Benke
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
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
  public int send(LogMessage logMessage) {
    boolean result;

    String formattedKey = options.getKey().getResolvedString(logMessage);
    String formattedTopic = options.getTopic().getResolvedString(logMessage);
    String formattedMessage = options.getTemplate().getResolvedString(logMessage);

    ProducerRecord<String,String> producerRecord = new ProducerRecord<String,String>(formattedTopic, formattedKey, formattedMessage);

    logger.debug("Outgoing message: " + producerRecord.toString());
    if (options.getSyncSend()) {
      result =  sendSynchronously(producerRecord);
    } else
      result = sendAsynchronously(producerRecord);

    if (result)
        return SUCCESS;
    else
        return ERROR;
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
