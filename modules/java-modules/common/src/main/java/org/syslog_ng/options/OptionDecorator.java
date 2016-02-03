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

public abstract class OptionDecorator implements Option {

	protected Option decoratedOption;

	public OptionDecorator(Option decoratedOption) {
		this.decoratedOption = decoratedOption;
	}

	public String getValue() {
		return decoratedOption.getValue();
	}

	public void validate() throws InvalidOptionException {
		decoratedOption.validate();
	}

	public String getName() {
		return decoratedOption.getName();
	}


	public Integer getValueAsInteger() {
		return decoratedOption.getValueAsInteger();
	}

	public String[] getValueAsStringList(String separator) {
		return decoratedOption.getValueAsStringList(separator);
	}

	public Boolean getValueAsBoolean() {
		return decoratedOption.getValueAsBoolean();
	}
}
