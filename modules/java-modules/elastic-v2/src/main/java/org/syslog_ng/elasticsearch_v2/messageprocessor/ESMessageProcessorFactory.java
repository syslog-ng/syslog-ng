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

package org.syslog_ng.elasticsearch_v2.messageprocessor;

import org.syslog_ng.elasticsearch_v2.ElasticSearchOptions;
import org.syslog_ng.elasticsearch_v2.client.esnative.ESNativeClient;
import org.syslog_ng.elasticsearch_v2.client.http.ESHttpClient;
import org.syslog_ng.elasticsearch_v2.messageprocessor.esnative.DummyProcessorNative;
import org.syslog_ng.elasticsearch_v2.messageprocessor.esnative.ESBulkNativeMessageProcessor;
import org.syslog_ng.elasticsearch_v2.messageprocessor.esnative.ESNativeMessageProcessor;
import org.syslog_ng.elasticsearch_v2.messageprocessor.esnative.ESSingleNativeMessageProcessor;
import org.syslog_ng.elasticsearch_v2.messageprocessor.http.HttpBulkMessageProcessor;
import org.syslog_ng.elasticsearch_v2.messageprocessor.http.HttpMessageProcessor;
import org.syslog_ng.elasticsearch_v2.messageprocessor.http.HttpSingleMessageProcessor;

public class ESMessageProcessorFactory {
	public static ESNativeMessageProcessor getMessageProcessor(ElasticSearchOptions options, ESNativeClient client) {
		int flush_limit = options.getFlushLimit();
		if (flush_limit > 1) {
			return new ESBulkNativeMessageProcessor(options, client);
		}
		if (flush_limit == -1) {
			return new DummyProcessorNative(options, client);
		}
		else {
			return new ESSingleNativeMessageProcessor(options, client);
		}
	}

	public static HttpMessageProcessor getMessageProcessor(ElasticSearchOptions options, ESHttpClient client) {
		int flush_limit = options.getFlushLimit();
		if (flush_limit > 1) {
			return new HttpBulkMessageProcessor(options, client);
		}
		else {
			return new HttpSingleMessageProcessor(options, client);
		}
	}
}
