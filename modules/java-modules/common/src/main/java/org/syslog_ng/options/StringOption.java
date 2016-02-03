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

import org.syslog_ng.LogDestination;
import org.syslog_ng.LogMessage;

public class StringOption implements Option {
	private LogDestination owner;
	private String name;
	private String defaultValue;

	public StringOption(LogDestination owner, String name, String defaultValue) {
		this.owner = owner;
		this.name = name;
		this.defaultValue = defaultValue;
	}

	public StringOption(LogDestination owner, String name) {
		this(owner, name, null);
	}

	public String getValue() {
		String result = defaultValue;
		String value = owner.getOption(name);
		if (value != null) {
			result = value;
		}
		return result;
	}

	public void validate() throws InvalidOptionException {
	}

	public void deinit() {
	}

	public String getResolvedString(LogMessage msg) {
		return getValue();
	}

	public String getName() {
		return name;
	}

	public Integer getValueAsInteger() {
		try {
			return Integer.parseInt(getValue());
		}
		catch (NumberFormatException e) {
			return null;
		}
	}

	public String[] getValueAsStringList(String separator) {
		String value = getValue();
		return value == null ? null : value.split(separator);
	}

	@Override
	public Boolean getValueAsBoolean() {
		return null;
	}
}
