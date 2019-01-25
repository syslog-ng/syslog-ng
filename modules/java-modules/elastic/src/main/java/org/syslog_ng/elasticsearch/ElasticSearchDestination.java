/*
 * Copyright (c) 2015 Balabit
 * Copyright (c) 2015 Viktor Juhasz <viktor.juhasz@balabit.com>
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

package org.syslog_ng.elasticsearch;

import org.syslog_ng.LogMessage;
import org.syslog_ng.LogTemplate;
import org.syslog_ng.StructuredLogDestination;
import org.syslog_ng.elasticsearch.client.ESClient;
import org.syslog_ng.elasticsearch.client.ESClientFactory;
import org.syslog_ng.elasticsearch.client.UnknownESClientModeException;
import org.syslog_ng.elasticsearch.messageprocessor.ESMessageProcessor;
import org.syslog_ng.elasticsearch.messageprocessor.ESMessageProcessorFactory;
import org.syslog_ng.elasticsearch.ElasticSearchOptions;
import org.syslog_ng.options.InvalidOptionException;
import org.syslog_ng.logging.SyslogNgInternalLogger;
import org.apache.log4j.Logger;
import org.elasticsearch.action.index.IndexRequest;

public class ElasticSearchDestination extends StructuredLogDestination {

	ESClient client;
	ESMessageProcessor msgProcessor;
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
		logger.warn("Using elasticsearch() destination (V1.x) is deprecated. This works as before but it may be removed in the future");
		boolean result = false;
		try {
			options.init();
			client = ESClientFactory.getESClient(options);
			msgProcessor = ESMessageProcessorFactory.getMessageProcessor(options, client);
			client.init();
			if (options.getClientMode().equals(ElasticSearchOptions.CLIENT_MODE_TRANSPORT) && options.getFlushLimit() > 1) {
				logger.warn("Using transport client mode with bulk message processing (flush_limit > 1) can cause high message dropping rate in case of connection broken, using node client mode is suggested");
			}
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
		if (opened){
			msgProcessor.init();
		}
		return opened;
	}

    private IndexRequest createIndexRequest(LogMessage msg) {
    	String formattedMessage = options.getMessageTemplate().getResolvedString(msg, getTemplateOptionsHandle(), LogTemplate.LTZ_SEND);
    	String customId = options.getCustomId().getResolvedString(msg, getTemplateOptionsHandle(), LogTemplate.LTZ_SEND);
    	String index = options.getIndex().getResolvedString(msg, getTemplateOptionsHandle(), LogTemplate.LTZ_SEND);
    	String type = options.getType().getResolvedString(msg, getTemplateOptionsHandle(), LogTemplate.LTZ_SEND);
    	logger.debug("Outgoing log entry, json='" + formattedMessage + "'");
    	return new IndexRequest(index, type, customId).source(formattedMessage);
    }

	@Override
	protected int send(LogMessage msg) {
		if (!client.isOpened()) {
			close();
			return SUCCESS;
		}
		if (msgProcessor.send(createIndexRequest(msg)))
			return SUCCESS;
		else
			return ERROR;
	}

	@Override
	protected void close() {
		if (opened) {
			msgProcessor.flush();
			msgProcessor.deinit();
			client.close();
			opened = false;
		}
	}

	@Override
	protected String getNameByUniqOptions() {
		return String.format("ElasticSearch,%s,%s", client.getClusterName(), options.getIndex().getValue());
	}

	@Override
	protected void deinit() {
		client.deinit();
		options.deinit();
	}
}
