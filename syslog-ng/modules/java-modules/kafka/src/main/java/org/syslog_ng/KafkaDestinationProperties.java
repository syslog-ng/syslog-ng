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

import org.apache.kafka.clients.producer.ProducerConfig;
import org.apache.kafka.common.serialization.StringSerializer;
import org.apache.log4j.Logger;
import org.syslog_ng.logging.SyslogNgInternalLogger;
import org.syslog_ng.kafka.KafkaDestinationOptions;

import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.util.Properties;

public class KafkaDestinationProperties {
    private KafkaDestinationOptions options;
    private Properties properties;
    private Logger logger;

    public KafkaDestinationProperties(KafkaDestinationOptions options) {
        this.options = options;
        logger = Logger.getLogger("root");
    }

    public void init() {
        String propertiesFile = options.getPropertiesFile();
        this.properties = new Properties();

        this.loadDefaultProperties();

        if (propertiesFile != null) {
            this.loadPropertiesFile(propertiesFile);
        }

        this.loadSyslogNgOptions();
    }

    public Properties getProperties() {
        return properties;
    }

    private void loadDefaultProperties() {
        this.properties.put(ProducerConfig.METADATA_FETCH_TIMEOUT_CONFIG, "10000");
    }

    private boolean loadPropertiesFile(String propertiesFile) {
        InputStream inputStream = this.openPropertiesFile(propertiesFile);

        if (inputStream != null) {
            return this.loadPropertiesFromStream(inputStream);
        }

        return false;
    }

    private InputStream openPropertiesFile(String propertiesFile) {
        InputStream inputStream = null;

        try {
            inputStream = new FileInputStream(propertiesFile);
            logger.debug(String.format("properties file successfully opened: %s", propertiesFile));
        } catch (FileNotFoundException e) {
            logger.error(String.format("unable to open properties file: %s", propertiesFile), e);
        }
        return inputStream;
    }

    private boolean loadPropertiesFromStream(InputStream inputStream) {
        Properties props = new Properties();

        try {
            props.load(inputStream);
            this.properties.putAll(props);
            logger.debug("properties successfully loaded");
            return true;
        } catch (IOException e) {
            logger.error("unable to load properties", e);
        }
        return false;
    }

    private void loadSyslogNgOptions() {
        this.properties.put(ProducerConfig.VALUE_SERIALIZER_CLASS_CONFIG, StringSerializer.class.getName());
        this.properties.put(ProducerConfig.KEY_SERIALIZER_CLASS_CONFIG,   StringSerializer.class.getName());
        this.properties.put(ProducerConfig.BOOTSTRAP_SERVERS_CONFIG,      this.options.getKafkaBootstrapServers());
    }
}
