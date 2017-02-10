/*
 * Copyright (c) 2016 Balabit
 * Copyright (c) 2016 Laszlo Budai <laszlo.budai@balabit.com>
 * Copyright (c) 2016 Viktor Tusa <tusavik@gmail.com>
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

package org.syslog_ng.elasticsearch_v2.messageprocessor.http;

import io.searchbox.core.Index;
import org.apache.log4j.Logger;
import org.syslog_ng.elasticsearch_v2.ElasticSearchOptions;
import org.syslog_ng.elasticsearch_v2.client.http.ESHttpClient;
import org.syslog_ng.elasticsearch_v2.messageprocessor.ESIndex;
import org.syslog_ng.elasticsearch_v2.messageprocessor.ESMessageProcessor;
import java.io.IOException;

public abstract class HttpMessageProcessor implements ESMessageProcessor {
	protected ElasticSearchOptions options;
	protected ESHttpClient client;
	protected Logger logger;

	public HttpMessageProcessor(ElasticSearchOptions options, ESHttpClient client) {
		this.options = options;
		this.client = client;
		this.logger = Logger.getRootLogger();
	}

	public void init() {
	}

	public void flush() throws IOException {

	}

	public void deinit() {

	}

	protected abstract boolean send(Index req);

	@Override
	public boolean send(ESIndex index) {
		Index req = new Index.Builder(index.getFormattedMessage()).index(index.getIndex()).type(index.getType()).id(index.getId()).build();
		return send(req);
	}

    public void onMessageQueueEmpty() {

    }
}
