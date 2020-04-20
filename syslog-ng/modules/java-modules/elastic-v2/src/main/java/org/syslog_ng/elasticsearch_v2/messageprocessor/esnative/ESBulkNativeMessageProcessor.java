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

package org.syslog_ng.elasticsearch_v2.messageprocessor.esnative;

import java.util.concurrent.TimeUnit;

import org.elasticsearch.action.bulk.BulkProcessor;
import org.elasticsearch.action.bulk.BulkRequest;
import org.elasticsearch.action.bulk.BulkResponse;
import org.elasticsearch.action.index.IndexRequest;
import org.elasticsearch.common.unit.TimeValue;
import org.syslog_ng.elasticsearch_v2.ElasticSearchOptions;
import org.syslog_ng.elasticsearch_v2.client.esnative.ESNativeClient;

public class ESBulkNativeMessageProcessor extends ESNativeMessageProcessor {
	private BulkProcessor bulkProcessor;

	private class BulkProcessorListener implements BulkProcessor.Listener {
		@Override
		public void beforeBulk(long executionId, BulkRequest request) {
			logger.debug("Start bulk processing, id='" + executionId + "'");
		}

		@Override
		public void afterBulk(long executionId, BulkRequest request,
				BulkResponse response) {
			logger.debug("Bulk processing finished successfully, id='" + executionId + "'"
					+ ", numberOfMessages='" + request.numberOfActions() + "'");
		}

		@Override
		public void afterBulk(long executionId, BulkRequest request,
				Throwable failure) {
			String errorMessage = "Bulk processing failed,";
			errorMessage += " id='" + executionId + "'";
			errorMessage += ", numberOfMessages='" + request.numberOfActions() + "'";
			errorMessage += ", error='" + failure.getMessage() + "'";
			logger.error(errorMessage);
		}
	}

	public ESBulkNativeMessageProcessor(ElasticSearchOptions options, ESNativeClient client) {
		super(options, client);
	}

	@Override
	protected boolean send(IndexRequest req) {
		bulkProcessor.add(req);
		return true;
	}

	@Override
	public void flush() {
		bulkProcessor.flush();
	}

	@Override
	public void init() {
		bulkProcessor = BulkProcessor.builder(
				client.getClient(),
				new BulkProcessorListener()
			)
			.setBulkActions(options.getFlushLimit())
			.setFlushInterval(new TimeValue(1000))
			.setConcurrentRequests(options.getConcurrentRequests())
			.build();
	}

	@Override
	public void deinit() {
		try {
			bulkProcessor.awaitClose(1, TimeUnit.SECONDS);
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
	}

}
