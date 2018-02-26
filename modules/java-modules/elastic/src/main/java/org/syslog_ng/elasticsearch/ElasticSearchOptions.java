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

import java.util.Arrays;
import java.util.HashSet;

import org.syslog_ng.LogDestination;
import org.syslog_ng.options.*;

public class ElasticSearchOptions {

	public static String SERVER = "server";
	public static String PORT = "port";
	public static String CLUSTER = "cluster";
	public static String INDEX = "index";
	public static String TYPE = "type";
	public static String MESSAGE_TEMPLATE = "message_template";
	public static String CUSTOM_ID = "custom_id";
	public static String FLUSH_LIMIT = "flush_limit";
	public static String CLIENT_MODE = "client_mode";
	public static String CONFIG_FILE = "resource";
	public static String CONCURRENT_REQUESTS = "concurrent_requests";

	public static String SERVER_DEFAULT = "localhost";
	public static String PORT_DEFAULT = "9300";
	public static String MESSAGE_TEMPLATE_DEFAULT = "$(format-json --scope rfc5424 --exclude DATE --key ISODATE)";
	public static String FLUSH_LIMIT_DEFAULT = "5000";
	public static String CLIENT_MODE_TRANSPORT = "transport";
	public static String CLIENT_MODE_NODE = "node";
	public static HashSet<String> CLIENT_MODES  = new HashSet<String>(Arrays.asList(CLIENT_MODE_TRANSPORT, CLIENT_MODE_NODE));

	public static String CLIENT_MODE_DEFAULT = CLIENT_MODE_TRANSPORT;
	public static String CONCURRENT_REQUESTS_DEFAULT = "1";

	private LogDestination owner;
	private Options options;

	public ElasticSearchOptions(LogDestination owner) {
		this.owner = owner;
		options = new Options();
		fillOptions();
	}

	public void init() throws InvalidOptionException {
		options.validate();
	}

	public void deinit() {
		options.deinit();
	}

	public Option getOption(String optionName) {
		return options.get(optionName);
	}

	public TemplateOption getMessageTemplate() {
		return options.getTemplateOption(MESSAGE_TEMPLATE);
	}

	public TemplateOption getCustomId() {
		return options.getTemplateOption(CUSTOM_ID);
	}

	public TemplateOption getIndex() {
		return options.getTemplateOption(INDEX);
	}

	public TemplateOption getType() {
		return options.getTemplateOption(TYPE);
	}

	public String[] getServerList() {
		return options.get(SERVER).getValueAsStringList(" ");
	}

	public int getPort() {
		return options.get(PORT).getValueAsInteger();
	}

	public String getCluster() {
		return options.get(CLUSTER).getValue();
	}

	public int getFlushLimit() {
		return options.get(FLUSH_LIMIT).getValueAsInteger();
	}

	public String getClientMode() {
		return options.get(CLIENT_MODE).getValue();
	}

	public String getConfigFile() {
		return options.get(CONFIG_FILE).getValue();
	}

        public int getConcurrentRequests() {
        	return options.get(CONCURRENT_REQUESTS).getValueAsInteger();
        }	

	private void fillOptions() {
		fillStringOptions();
		fillTemplateOptions();
	}

	private void fillTemplateOptions() {
		options.put(new TemplateOption(owner.getConfigHandle(), new RequiredOptionDecorator(new StringOption(owner, INDEX))));
		options.put(new TemplateOption(owner.getConfigHandle(), new RequiredOptionDecorator(new StringOption(owner, TYPE))));
		options.put(new TemplateOption(owner.getConfigHandle(), new StringOption(owner, MESSAGE_TEMPLATE, MESSAGE_TEMPLATE_DEFAULT)));
		options.put(new TemplateOption(owner.getConfigHandle(), new StringOption(owner, CUSTOM_ID)));
	}

	private void fillStringOptions() {
		options.put(new StringOption(owner, SERVER, SERVER_DEFAULT));
		options.put(new PortCheckDecorator(new StringOption(owner, PORT, PORT_DEFAULT)));
		options.put(new StringOption(owner, CLUSTER));
		options.put(new IntegerRangeCheckOptionDecorator(new StringOption(owner, FLUSH_LIMIT, FLUSH_LIMIT_DEFAULT), -1, Integer.MAX_VALUE));
		options.put(new EnumOptionDecorator(new StringOption(owner, CLIENT_MODE, CLIENT_MODE_DEFAULT), CLIENT_MODES));
		options.put(new StringOption(owner, CONFIG_FILE));
		options.put(new IntegerRangeCheckOptionDecorator(new StringOption(owner, CONCURRENT_REQUESTS, CONCURRENT_REQUESTS_DEFAULT), 0, Integer.MAX_VALUE));
	}
}
