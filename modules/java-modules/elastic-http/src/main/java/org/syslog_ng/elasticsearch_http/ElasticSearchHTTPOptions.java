/*
 * Copyright (c) 2015 Balabit
 * Copyright (c) 2015 Viktor Juhasz <viktor.juhasz@balabit.com>
 * Copyright (c) 2016 Viktor Tusa <tusavik@gmail.com>
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

package org.syslog_ng.elasticsearch_http;

import java.util.Arrays;
import java.util.HashSet;

import org.syslog_ng.LogDestination;
import org.syslog_ng.options.*;

public class ElasticSearchHTTPOptions {

	public static String CLUSTER_URL = "cluster_url";
	public static String CLUSTER = "cluster";
	public static String INDEX = "index";
	public static String TYPE = "type";
	public static String MESSAGE_TEMPLATE = "message_template";
	public static String CUSTOM_ID = "custom_id";
	public static String FLUSH_LIMIT = "flush_limit";
	public static String CONFIG_FILE = "resource";
	public static String CONCURRENT_REQUESTS = "concurrent_requests";

	public static String CLUSTER_URL_DEFAULT = "http://localhost:9200";
	public static String MESSAGE_TEMPLATE_DEFAULT = "$(format-json --scope rfc5424 --exclude DATE --key ISODATE)";
	public static String FLUSH_LIMIT_DEFAULT = "5000";

	public static String CONCURRENT_REQUESTS_DEFAULT = "1";

	private LogDestination owner;
	private Options options;

	public ElasticSearchHTTPOptions(LogDestination owner) {
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

	public String getClusterUrl() {
		return options.get(CLUSTER_URL).getValue();
	}

	public String getCluster() {
		return options.get(CLUSTER).getValue();
	}

	public int getFlushLimit() {
		return options.get(FLUSH_LIMIT).getValueAsInteger();
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
		options.put(new StringOption(owner, CLUSTER));
		options.put(new StringOption(owner, CLUSTER_URL, CLUSTER_URL_DEFAULT));
		options.put(new IntegerRangeCheckOptionDecorator(new StringOption(owner, FLUSH_LIMIT, FLUSH_LIMIT_DEFAULT), -1, Integer.MAX_VALUE));
		options.put(new StringOption(owner, CONFIG_FILE));
		options.put(new IntegerRangeCheckOptionDecorator(new StringOption(owner, CONCURRENT_REQUESTS, CONCURRENT_REQUESTS_DEFAULT), 0, Integer.MAX_VALUE));
	}
}
