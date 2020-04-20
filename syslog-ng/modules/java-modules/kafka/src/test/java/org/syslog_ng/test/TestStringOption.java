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

import org.junit.Before;
import org.junit.Test;
import org.syslog_ng.options.Option;
import org.syslog_ng.options.StringOption;

import static org.junit.Assert.*;

public class TestStringOption extends TestOption {


	@Before
	public void setUp() throws Exception {
		super.setUp();
		options.put("cluster", "syslog-ng");
		options.put("port", "9300");
		options.put("server", "localhost remote_host");
	}

	@Test
	public void testExisting() {
		Option stringOption = new StringOption(owner, "cluster");
		assertInitOptionSuccess(stringOption);
		assertEquals("syslog-ng", stringOption.getValue());
	}

	@Test
	public void testExistingWithDefaultValue() {
		Option stringOption = new StringOption(owner, "cluster", "something else");
		assertInitOptionSuccess(stringOption);
		assertEquals("syslog-ng", stringOption.getValue());
	}

	@Test
	public void testNonExisting() {
		Option stringOption = new StringOption(owner, "badcluster");
		assertInitOptionSuccess(stringOption);
		assertEquals(null, stringOption.getValue());
	}

	@Test
	public void testNonExistingWithDefaultValue() {
		Option stringOption = new StringOption(owner, "badcluster", "something else");
		assertInitOptionSuccess(stringOption);
		assertEquals("something else", stringOption.getValue());
	}

	@Test
	public void testWithValidInteger() {
		Option stringOption = new StringOption(owner, "port");
		assertInitOptionSuccess(stringOption);
		assertEquals(new Integer(9300), stringOption.getValueAsInteger());
	}

	@Test
	public void testWithInValidInteger() {
		Option stringOption = new StringOption(owner, "cluster");
		assertInitOptionSuccess(stringOption);
		assertEquals(null, stringOption.getValueAsInteger());
	}

	@Test
	public void testWithValidStringList() {
		Option stringOption = new StringOption(owner, "server");
		assertInitOptionSuccess(stringOption);
		assertArrayEquals(new String[] {"localhost", "remote_host"} , stringOption.getValueAsStringList(" "));
	}

	@Test
	public void testWithInValidStringList() {
		Option stringOption = new StringOption(owner, "badcluster");
		assertInitOptionSuccess(stringOption);
		assertArrayEquals(null, stringOption.getValueAsStringList(" "));
	}

	@Test
	public void testWithSingleValidStringList() {
		Option stringOption = new StringOption(owner, "cluster");
		assertInitOptionSuccess(stringOption);
		assertArrayEquals(new String[]{"syslog-ng"}, stringOption.getValueAsStringList(" "));
	}

}
