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
import org.syslog_ng.ThreadedStructuredLogDestination;
import org.syslog_ng.elasticsearch_v2.client.ESClient;
import org.syslog_ng.elasticsearch_v2.client.ESClientFactory;
import org.syslog_ng.elasticsearch_v2.client.UnknownESClientModeException;
import org.syslog_ng.elasticsearch_v2.messageprocessor.ESIndex;
import org.syslog_ng.options.InvalidOptionException;
import org.syslog_ng.logging.SyslogNgInternalLogger;
import org.apache.log4j.Logger;

public class ElasticSearchDestination extends ThreadedStructuredLogDestination {

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

                        setBatchLines(options.getFlushLimit());
                        setBatchTimeout(options.getFlushTimeout());

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

	private int send_batch(ESIndex index) {
		if (client.send(index))
			return WORKER_INSERT_RESULT_QUEUED;
		else
			return WORKER_INSERT_RESULT_ERROR;
	}

	private int send_single(ESIndex index) {
		if (client.send(index))
			return WORKER_INSERT_RESULT_SUCCESS;
		else
			return WORKER_INSERT_RESULT_ERROR;
	}

	@Override
	protected int send(LogMessage msg) {
		if (!client.isOpened()) {
			return WORKER_INSERT_RESULT_NOT_CONNECTED;
		}

                ESIndex index = createIndexRequest(msg);
		if (options.getFlushLimit() > 1)
			return send_batch(index);
		else
			return send_single(index);
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
	public int flush() {
		if (client.flush())
			return WORKER_INSERT_RESULT_SUCCESS;
		else
			return WORKER_INSERT_RESULT_ERROR;
	}
}
