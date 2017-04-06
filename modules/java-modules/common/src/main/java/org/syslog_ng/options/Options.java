/*
 * Copyright (c) 2015 Balabit
 * Copyright (c) 2015 Viktor Juhasz <viktor.juhasz@balabit.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

package org.syslog_ng.options;

import java.util.HashMap;

public class Options {
	private HashMap<String, Option> stringOptions;
	private HashMap<String, TemplateOption> templateOptions;

	public Options() {
		stringOptions = new HashMap<String, Option>();
		templateOptions = new HashMap<String, TemplateOption>();
	}

	public void put(Option stringOption) {
		stringOptions.put(stringOption.getName(), stringOption);
	}

	public void put(TemplateOption templateOption) {
		templateOptions.put(templateOption.getName(), templateOption);
	}

	public void validate() throws InvalidOptionException {
		validateStringOptions();
		validateTemplateOptions();
	}

	public void deinit() {
		for (String key : templateOptions.keySet()) {
			TemplateOption option = templateOptions.get(key);
			option.deinit();
		}
	}

	private void validateTemplateOptions() throws InvalidOptionException {
		for (String key : templateOptions.keySet()) {
			TemplateOption option = templateOptions.get(key);
			option.validate();
		}
	}

	public Option getStringOption(String name) {
		return stringOptions.get(name);
	}

	public TemplateOption getTemplateOption(String name) {
		return templateOptions.get(name);
	}

	public Option get(String name) {
		Option result = getStringOption(name);
		if (result == null) {
			result = getTemplateOption(name);
		}
		return result;
	}

	private void validateStringOptions() throws InvalidOptionException {
		for (String key : stringOptions.keySet()) {
			Option option = stringOptions.get(key);
			option.validate();
		}
	}
}
