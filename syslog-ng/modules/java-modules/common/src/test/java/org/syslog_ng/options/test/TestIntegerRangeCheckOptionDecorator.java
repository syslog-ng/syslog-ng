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

package org.syslog_ng.options.test;

import org.junit.Before;
import org.junit.Test;
import org.syslog_ng.options.IntegerRangeCheckOptionDecorator;
import org.syslog_ng.options.Option;
import org.syslog_ng.options.StringOption;

public class TestIntegerRangeCheckOptionDecorator extends TestOption {
	Option stringOption;
	Option decorator;

	@Before
	public void setUp() throws Exception {
		super.setUp();
		stringOption = new StringOption(owner, "range");
		decorator = new IntegerRangeCheckOptionDecorator(stringOption, 0, 1000);
	}

	@Test
	public void testNormal() {
		for (int i = 0; i < 1001; i++) {
			options.put("range", new Integer(i).toString());
			assertInitOptionSuccess(decorator);
		}
	}

	@Test
	public void testInvalid() {
		options.put("range", "-1");
		assertInitOptionFailed(decorator, "option range should be between 0 and 1000");

		options.put("range", "1001");
		assertInitOptionFailed(decorator, "option range should be between 0 and 1000");

		options.put("range", "0.1");
		assertInitOptionFailed(decorator, "option range must be numerical");

		options.put("range", "aaa");
		assertInitOptionFailed(decorator, "option range must be numerical");

		options.remove("range");
		assertInitOptionFailed(decorator, "option range must be numerical");
	}

}
