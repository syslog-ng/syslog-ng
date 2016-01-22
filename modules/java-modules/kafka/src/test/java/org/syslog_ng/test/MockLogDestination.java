/*
 * Copyright (c) 2015 Balabit
 * Copyright (c) 2015 Tibor Benke
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

package org.syslog_ng.test;

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

	@Override
	protected String getNameByUniqOptions() {
		// TODO Auto-generated method stub
		return "Dummy";
	}
}
