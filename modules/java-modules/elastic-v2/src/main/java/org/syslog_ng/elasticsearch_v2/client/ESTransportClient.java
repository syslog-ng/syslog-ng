/*
 * Copyright (c) 2016 BalaBit IT Ltd, Budapest, Hungary
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

package org.syslog_ng.elasticsearch_v2.client;

import java.net.InetSocketAddress;

import org.elasticsearch.client.Client;
import org.elasticsearch.client.transport.TransportClient;
import org.elasticsearch.common.settings.Settings;
import org.elasticsearch.common.settings.Settings.Builder;
import org.elasticsearch.common.transport.InetSocketTransportAddress;
import org.syslog_ng.elasticsearch_v2.ElasticSearchOptions;

public class ESTransportClient extends ESClient {
	private Settings settings;
	private TransportClient transportClient;

	public ESTransportClient(ElasticSearchOptions options) {
		super(options);
	}

	public Client createClient() {
		String clusterName = options.getCluster();
		Builder settingsBuilder = Settings.builder();
		settingsBuilder = settingsBuilder.put("client.transport.sniff", true);
		        
		if (clusterName != null) {
			settingsBuilder = settingsBuilder.put("cluster.name", clusterName);
		}

		settings = settingsBuilder.build();

		String[] servers =  options.getServerList();

		transportClient = TransportClient.builder().settings(settings).build();

        for (String server : servers) {
            InetSocketAddress inetSocketAddress = new InetSocketAddress(server, options.getPort());
            if (inetSocketAddress.isUnresolved()) {
                logger.warn("Cannot resolve server, address = " + server + ", skipping from server list");
            } else {
                transportClient.addTransportAddress(new InetSocketTransportAddress(inetSocketAddress));
            }
        }
		return transportClient;
	}

	public void close() {
		getClient().close();
		resetClient();
	}

	@Override
	public boolean isOpened() {
		return !transportClient.connectedNodes().isEmpty();
	}

	@Override
	public void deinit() {

	}
}
