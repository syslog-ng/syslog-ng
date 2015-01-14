/*
 * Copyright (c) 2014 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2014 Viktor Juhasz <viktor.juhasz@balabit.com>
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
package org.syslog_ng;

public class DummyTextDestination extends TextLogDestination {

	private String name;

	public DummyTextDestination(long arg0) {
		super(arg0);
	}

	public void deinit() {
		System.out.println("Deinit");
	}

	public boolean flush() {
		System.out.println("Flush");
		return true;
	}

	public boolean init() {
		name = getOption("name");
		if (name == null) {
			System.err.println("Name is a required option for this destination");
			return false;
		}
		System.out.println("Init " + name);
		return true;
	}

	public boolean queue(String arg0) {
		System.out.println("Incoming message: " + arg0);
		return true;
	}
}
