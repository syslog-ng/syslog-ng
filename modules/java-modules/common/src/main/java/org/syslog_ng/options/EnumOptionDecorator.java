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

import java.util.Set;

public class EnumOptionDecorator extends OptionDecorator {
	private Set<String> possibleValues;

	public EnumOptionDecorator(Option decoratedOption, Set<String> possibleValues) {
		super(decoratedOption);
		this.possibleValues = possibleValues;
	}

	public String getPossibleValuesAsString() {
		String result = "[";
		for (String value: possibleValues) {
			if (result.length() > 1) {
				result += ", ";
			}
			result += "'" + value + "'";
		}
		result += "]";
		return result;
	}

	public void validate() throws InvalidOptionException {
		decoratedOption.validate();
		String value = decoratedOption.getValue();
		if (!possibleValues.contains(value)) {
			throw new InvalidOptionException("option " + decoratedOption.getName() + " must be one of the following values: " + getPossibleValuesAsString());
		}
	}

}
