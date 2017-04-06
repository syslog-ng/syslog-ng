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

package org.syslog_ng.logging.test;

import java.util.Hashtable;

import org.syslog_ng.LogDestination;

public class MockLogDestination extends LogDestination {

	private Hashtable<String, String> options;

	public MockLogDestination(Hashtable<String, String> options) {
		super(0);
		this.options = options;
	}

	@Override
	protected void close() {
		// TODO Auto-generated method stub

	}

	@Override
	public String getOption(String optionName) {
		return options.get(optionName);
	}

	@Override
	public String getNameByUniqOptions() {
		return "Dummy";
	}

	@Override
	protected boolean isOpened() {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	protected boolean open() {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	protected void deinit() {
		// TODO Auto-generated method stub

	}

	@Override
	protected boolean init() {
		// TODO Auto-generated method stub
		return false;
	}

}
