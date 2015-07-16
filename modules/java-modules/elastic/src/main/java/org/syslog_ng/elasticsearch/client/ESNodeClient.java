/*
 * Copyright (c) 2015 BalaBit IT Ltd, Budapest, Hungary
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

package org.syslog_ng.elasticsearch.client;

import static org.elasticsearch.node.NodeBuilder.nodeBuilder;

import java.io.File;
import java.net.MalformedURLException;
import java.net.URL;

import org.elasticsearch.client.Client;
import org.elasticsearch.common.settings.ImmutableSettings.Builder;
import org.elasticsearch.common.settings.SettingsException;
import org.elasticsearch.node.Node;
import org.elasticsearch.node.NodeBuilder;
import org.syslog_ng.elasticsearch.ElasticSearchOptions;

public class ESNodeClient extends ESClient {
	private Node node;

	public ESNodeClient(ElasticSearchOptions options) {
		super(options);
	}

	@Override
	public void close() {
		getClient().close();
	}

	private NodeBuilder createNodeBuilder(String cluster) {
		NodeBuilder result = nodeBuilder().data(false)
				  .client(true)
				  .loadConfigSettings(false);

		if (cluster != null) {
			result = result.clusterName(cluster);
		}
		return result;
	}

	private void loadConfigFile(String cfgFile, NodeBuilder nodeBuilder) {
		if (cfgFile == null || cfgFile.isEmpty()) {
			return;
		}
		try {
			URL url = new File(cfgFile).toURI().toURL();
			Builder builder = nodeBuilder().settings().loadFromUrl(url);
			nodeBuilder = nodeBuilder.settings(builder);
		} catch (MalformedURLException e) {
			logger.warn("Bad filename format, filename = '" + cfgFile + "'");
		} catch (SettingsException e) {
			logger.warn("Can't load settings from file, file = '" + cfgFile + "', reason = '" + e.getMessage() + "'");
		}
	}

	@Override
	public Client createClient() {
		NodeBuilder nodeBuilder = createNodeBuilder(options.getCluster());
		nodeBuilder.settings().put("discovery.initial_state_timeout", "5s");
		loadConfigFile(options.getConfigFile(), nodeBuilder);
		node = nodeBuilder.node();
	    return node.client();
	}

	@Override
	public boolean isOpened() {
		return true;
	}

	@Override
	public void deinit() {
		node.close();
	}
}
