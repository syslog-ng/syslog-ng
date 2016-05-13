/*
 * Copyright (c) 2016 Balabit
 * Copyright (c) 2016 Laszlo Budai <laszlo.budai@balabit.com>
 *
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

package org.syslog_ng.elasticsearch_v2.client.http;

import io.searchbox.client.JestClient;
import io.searchbox.client.JestClientFactory;
import io.searchbox.client.JestResult;
import io.searchbox.client.config.HttpClientConfig;
import io.searchbox.cluster.NodesInfo;
import org.apache.log4j.Logger;
import org.syslog_ng.elasticsearch_v2.ElasticSearchOptions;
import org.syslog_ng.elasticsearch_v2.client.ESClient;
import org.syslog_ng.elasticsearch_v2.messageprocessor.ESIndex;
import org.syslog_ng.elasticsearch_v2.messageprocessor.ESMessageProcessorFactory;
import org.syslog_ng.elasticsearch_v2.messageprocessor.http.HttpMessageProcessor;

import java.io.IOException;

public class ESHttpClient implements ESClient {
	private ElasticSearchOptions options;
	private JestClient client;
	private HttpMessageProcessor messageProcessor;
	private Logger logger;

	public ESHttpClient(ElasticSearchOptions options) {
		this.options = options;
		logger = Logger.getRootLogger();
		messageProcessor = ESMessageProcessorFactory.getMessageProcessor(options, this);
	}

	private JestClient createClient() {
		String connectionUrl = options.getClusterUrl();
		JestClientFactory clientFactory = new JestClientFactory();
		clientFactory.setHttpClientConfig(new HttpClientConfig
				.Builder(connectionUrl)
				.multiThreaded(false)
				.build());
		return clientFactory.getObject();
	}

	public JestClient getClient() {
		return client;
	}

	@Override
	public boolean open() {
		if (client == null) {
			client = createClient();
		}
		messageProcessor.init();
		return true;
	}

	@Override
	public void close() {
		messageProcessor.flush();
	}

	@Override
	public boolean isOpened() {
		return true;
	}

	@Override
	public void init() {
		this.client = createClient();
	}

	@Override
	public void deinit() {
		client.shutdownClient();
		options.deinit();
	}

	@Override
	public boolean send(ESIndex index) {
		return messageProcessor.send(index);
	}

	@Override
	public String getClusterName() {
		NodesInfo nodesinfo = new NodesInfo.Builder().build();
		String clusterName;
		try {
			JestResult result = client.execute(nodesinfo);
			clusterName = result.getValue("cluster_name").toString();
		} catch (IOException e) {
			clusterName = new String();
		}
		return clusterName;
	}
}
