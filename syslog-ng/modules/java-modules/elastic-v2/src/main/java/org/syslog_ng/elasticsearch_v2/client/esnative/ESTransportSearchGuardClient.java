/*
 * Copyright (c) 2016 Balabit
 * Copyright (c) 2016 Viktor Juhasz <viktor.juhasz@balabit.com>
 * Copyright (c) 2016 Zoltan Pallagi <zoltan.pallagi@balabit.com>
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

import java.util.ArrayList;
import org.elasticsearch.client.Client;
import org.elasticsearch.plugins.Plugin;
import com.floragunn.searchguard.ssl.SearchGuardSSLPlugin;
import org.syslog_ng.elasticsearch_v2.ElasticSearchOptions;
import org.syslog_ng.elasticsearch_v2.client.esnative.ESTransportClient;

public class ESTransportSearchGuardClient extends ESTransportClient {

	public ESTransportSearchGuardClient(ElasticSearchOptions options) {
		super(options);
	}
	
	@Override
    public Client createClient() {
	    ArrayList<Class<? extends Plugin>> plugins = new ArrayList<Class<? extends Plugin>>();    
        plugins.add(SearchGuardSSLPlugin.class);

        super.createClient(plugins);
        return transportClient;
    }
}
