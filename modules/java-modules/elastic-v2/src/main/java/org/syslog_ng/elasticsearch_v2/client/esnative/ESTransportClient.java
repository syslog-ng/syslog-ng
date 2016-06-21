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

package org.syslog_ng.elasticsearch_v2.client.esnative;

import java.net.InetSocketAddress;
import java.util.Iterator;
import java.util.List;

import org.elasticsearch.client.Client;
import org.elasticsearch.client.transport.TransportClient;
import org.elasticsearch.common.settings.Settings;
import org.elasticsearch.common.settings.Settings.Builder;
import org.elasticsearch.common.transport.InetSocketTransportAddress;
import org.elasticsearch.plugins.Plugin;
import org.syslog_ng.elasticsearch_v2.ElasticSearchOptions;
import org.syslog_ng.elasticsearch_v2.client.esnative.ESNativeClient;

public class ESTransportClient extends ESNativeClient {
	private Settings settings;
	protected TransportClient transportClient;

	public ESTransportClient(ElasticSearchOptions options) {
		super(options);
	}

	private void validate() {
		if (options.getFlushLimit() > 1) {
			logger.warn("Using transport based client mode with bulk message processing (flush_limit > 1) can cause high message dropping rate in case of connection broken, using node client mode is suggested");
		}
	}

    @Override
    public Client createClient() {
        return createClient(null);        
    }

	protected Client createClient(List<Class<? extends Plugin>> plugins) {
	    settings = initSettingsBuilder();		

	    org.elasticsearch.client.transport.TransportClient.Builder transportClientBuilder = TransportClient.builder().settings(settings);
		
        if (plugins != null) {
            Iterator<Class<? extends Plugin>> iterator = plugins.iterator();
            while (iterator.hasNext()) {
                transportClientBuilder.addPlugin(iterator.next());
            }
        }
		
		transportClient = transportClientBuilder.build();
		
		addServersToClient(transportClient);

		validate();

		return transportClient;
	}

	@Override
	public boolean isOpened() {
		return !transportClient.connectedNodes().isEmpty();
	}

	@Override
	public void deinit() {

	}
	
	protected Settings initSettingsBuilder() {
        Builder settingsBuilder = Settings.builder();
        String clusterName = options.getCluster();
        loadConfigFile(options.getConfigFile(), settingsBuilder);
        settingsBuilder = settingsBuilder.put("client.transport.sniff", true);
                
        if (clusterName != null) {
            settingsBuilder = settingsBuilder.put("cluster.name", clusterName);
        }

        return settingsBuilder.build();
	}
	
	protected void addServersToClient(TransportClient transportClient) {
	    String[] servers =  options.getServerList();
        for (String server : servers) {
            InetSocketAddress inetSocketAddress = new InetSocketAddress(server, options.getPort());
            if (inetSocketAddress.isUnresolved()) {
                logger.warn("Cannot resolve server, address = " + server + ", skipping from server list");
            } else {
                transportClient.addTransportAddress(new InetSocketTransportAddress(inetSocketAddress));
            }
        }
	}
}
