/*
 * Copyright (c) 2016 Balabit
 * Copyright (c) 2016 Viktor Juhasz <viktor.juhasz@balabit.com>
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

package org.syslog_ng.elasticsearch_v2;

import org.syslog_ng.LogMessage;
import org.syslog_ng.LogTemplate;
import org.syslog_ng.StructuredLogDestination;
import org.syslog_ng.elasticsearch_v2.client.ESClient;
import org.syslog_ng.elasticsearch_v2.client.ESClientFactory;
import org.syslog_ng.elasticsearch_v2.client.UnknownESClientModeException;
import org.syslog_ng.elasticsearch_v2.messageprocessor.ESIndex;
import org.syslog_ng.options.InvalidOptionException;
import org.syslog_ng.logging.SyslogNgInternalLogger;
import org.apache.log4j.Logger;

public class ElasticSearchDestination extends StructuredLogDestination {

	ESClient client;
	ElasticSearchOptions options;
	Logger logger;

	boolean opened;

	public ElasticSearchDestination(long handle) {
		super(handle);
		logger = Logger.getRootLogger();
		SyslogNgInternalLogger.register(logger);
		options = new ElasticSearchOptions(this);
	}

	@Override
	protected boolean init() {
		boolean result = false;
		try {
			options.init();
			client = ESClientFactory.getESClient(options);
			client.init();
			result = true;
		}
		catch (InvalidOptionException | UnknownESClientModeException e){
			logger.error(e.getMessage());
			return false;
		}
		return result;
	}

	@Override
	protected boolean isOpened() {
		return opened;
	}

	@Override
	protected boolean open() {
		opened = client.open();
		return opened;
	}

    private ESIndex createIndexRequest(LogMessage msg) {
    	String formattedMessage = options.getMessageTemplate().getResolvedString(msg, getTemplateOptionsHandle(), LogTemplate.LTZ_SEND);
    	String customId = options.getCustomId().getResolvedString(msg, getTemplateOptionsHandle(), LogTemplate.LTZ_SEND);
    	String index = options.getIndex().getResolvedString(msg, getTemplateOptionsHandle(), LogTemplate.LTZ_SEND);
    	String type = options.getType().getResolvedString(msg, getTemplateOptionsHandle(), LogTemplate.LTZ_SEND);
    	logger.debug("Outgoing log entry, json='" + formattedMessage + "'");

		return new ESIndex.Builder().formattedMessage(formattedMessage).index(index).id(customId).type(type).build();
    }

	@Override
	protected boolean send(LogMessage msg) {
		if (!client.isOpened()) {
			close();
			return false;
		}
		return client.send(createIndexRequest(msg));
	}

	@Override
	protected void close() {
		if (opened) {
			client.close();
			opened = false;
		}
	}

	@Override
	protected String getNameByUniqOptions() {
		return String.format("ElasticSearch_v2,%s,%s", client.getClusterName(), options.getIndex().getValue());
	}

	@Override
	protected void deinit() {
		client.deinit();
		options.deinit();
	}

	@Override
	public boolean flush() {
	    return client.flush();
	}
}
