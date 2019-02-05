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

import org.syslog_ng.LogMessage;
import org.syslog_ng.LogTemplate;

public class TemplateOption extends OptionDecorator {
	private LogTemplate template;
	private String strTemplate;
	private long configHandle;

	private native boolean isNamedTemplate(long configHandle, String template_name);

	public TemplateOption(long configHandle, Option decoratedOption) {
		super(decoratedOption);
		this.configHandle = configHandle;
	}

	@Override
	public void validate() throws InvalidOptionException {
		decoratedOption.validate();
		strTemplate = decoratedOption.getValue();
		if (strTemplate != null) {
			if (isNamedTemplate(configHandle, strTemplate))
				throw new InvalidOptionException("Named templates not supported: '" + strTemplate + "'");

			template = new LogTemplate(configHandle);
			if(!template.compile(strTemplate)) {
				throw new InvalidOptionException("Can't compile template: '" + strTemplate + "'");
			}
		}
	}

	public void deinit() {
		if (template != null) {
			template.release();
			template = null;
		}
	}

	public String getResolvedString(LogMessage msg) {
		if (template != null)
			return template.format(msg);
		return null;
	}

	public String getResolvedString(LogMessage msg, long templateOptionsHandle, int timezone) {
		if (template != null)
			return template.format(msg, templateOptionsHandle, timezone);
		return null;
	}

	public String getResolvedString(LogMessage msg, long templateOptionsHandle) {
		if (template != null)
			return template.format(msg, templateOptionsHandle);
		return null;
	}
}
