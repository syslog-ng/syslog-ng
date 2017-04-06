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

import org.syslog_ng.LogDestination;
import org.syslog_ng.options.*;
import org.syslog_ng.logging.*;

public class KafkaDestinationOptions {
    public static String TOPIC = "topic";
    public static String KEY = "key";
    public static String TEMPLATE = "template";
    public static String KAFKA_BOOTSTRAP_SERVERS = "kafka_bootstrap_servers";
    public static String PROPERTIES_FILE = "properties_file";
    public static String SYNC_SEND = "sync_send";

    public static String SYNC_SEND_DEFAULT = "false";
    public static String TEMPLATE_DEFAULT = "${ISODATE} ${HOST} ${MSGHDR}${MSG}\n";
    public static String KEY_DEFAULT = null;

    private LogDestination owner;
    private Options options;

    public KafkaDestinationOptions(LogDestination owner) {
        this.owner = owner;
        options = new Options();
        fillOptions();
    }

    public void init() throws InvalidOptionException {
        options.validate();
    }

    public void deinit() {
        options.deinit();
    }

    public boolean getSyncSend() {
        return options.get(SYNC_SEND).getValueAsBoolean();
    }

    public TemplateOption getTopic() {
        return options.getTemplateOption(TOPIC);
    }

    public TemplateOption getKey() {
        return options.getTemplateOption(KEY);
    }

    public TemplateOption getTemplate() {
        return options.getTemplateOption(TEMPLATE);
    }

    public String getKafkaBootstrapServers() {
        return options.get(KAFKA_BOOTSTRAP_SERVERS).getValue();
    }

    public String getPropertiesFile() {
        return options.get(PROPERTIES_FILE).getValue();
    }

    private void fillOptions() {
        fillStringOptions();
        fillTemplateOptions();
        fillBooleanOptions();
    }

    private void fillBooleanOptions() {
        options.put(new BooleanOptionDecorator(new StringOption(owner, SYNC_SEND, SYNC_SEND_DEFAULT)));
    }

    private void fillTemplateOptions() {
        options.put(new TemplateOption(owner.getConfigHandle(), new RequiredOptionDecorator(new StringOption(owner, TOPIC))));
        options.put(new TemplateOption(owner.getConfigHandle(), new StringOption(owner, KEY, KEY_DEFAULT)));
        options.put(new TemplateOption(owner.getConfigHandle(), new StringOption(owner, TEMPLATE, TEMPLATE_DEFAULT)));
    }

    private void fillStringOptions() {
        options.put(new RequiredOptionDecorator(new StringOption(owner, KAFKA_BOOTSTRAP_SERVERS)));
        options.put(new StringOption(owner, PROPERTIES_FILE));
    }
}

